/* ncdu - NCurses Disk Usage

  Copyright (c) Yorhel

  Permission is hereby granted, free of charge, to any person obtaining
  a copy of this software and associated documentation files (the
  "Software"), to deal in the Software without restriction, including
  without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Software, and to
  permit persons to whom the Software is furnished to do so, subject to
  the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
  CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
  TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#include "util.h"

#include <string.h>
#include <stdlib.h>
#include <ncurses.h>
#include <stdarg.h>
#include <unistd.h>
#include <pwd.h>

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

int uic_theme = 0;
int winrows, wincols;
int subwinr, subwinc;
static char thou_sep;


void die(const char *fmt, ...) {
  va_list arg;
  va_start(arg, fmt);
  vfprintf(stderr, fmt, arg);
  va_end(arg);
  exit(1);
}


char *cropstr(const char *from, int s) {
  static char dat[4096];
  int i, j, o = strlen(from);
  if(o < s) {
    strcpy(dat, from);
    return dat;
  }
  j=s/2-3;
  for(i=0; i<j; i++)
    dat[i] = from[i];
  dat[i] = '.';
  dat[++i] = '.';
  dat[++i] = '.';
  j=o-s;
  while(++i<s)
    dat[i] = from[j+i];
  dat[s] = '\0';
  return dat;
}


float formatsize(int64_t from, const char **unit) {
  float r = from;
  if (si) {
    if(r < 1000.0f)   { *unit = " B"; }
    else if(r < 1e6f) { *unit = "kB"; r/=1e3f; }
    else if(r < 1e9f) { *unit = "MB"; r/=1e6f; }
    else if(r < 1e12f){ *unit = "GB"; r/=1e9f; }
    else if(r < 1e15f){ *unit = "TB"; r/=1e12f; }
    else if(r < 1e18f){ *unit = "PB"; r/=1e15f; }
    else              { *unit = "EB"; r/=1e18f; }
  }
  else {
    if(r < 1000.0f)      { *unit = "  B"; }
    else if(r < 1023e3f) { *unit = "KiB"; r/=1024.0f; }
    else if(r < 1023e6f) { *unit = "MiB"; r/=1048576.0f; }
    else if(r < 1023e9f) { *unit = "GiB"; r/=1073741824.0f; }
    else if(r < 1023e12f){ *unit = "TiB"; r/=1099511627776.0f; }
    else if(r < 1023e15f){ *unit = "PiB"; r/=1125899906842624.0f; }
    else                 { *unit = "EiB"; r/=1152921504606846976.0f; }
  }
  return r;
}


void printsize(enum ui_coltype t, int64_t from) {
  const char *unit;
  float r = formatsize(from, &unit);
  uic_set(t == UIC_HD ? UIC_NUM_HD : t == UIC_SEL ? UIC_NUM_SEL : UIC_NUM);
  printw("%5.1f", r);
  addchc(t, ' ');
  addstrc(t, unit);
}


char *fullsize(int64_t from) {
  static char dat[26]; /* max: 9.223.372.036.854.775.807  (= 2^63-1) */
  char tmp[26];
  int64_t n = from;
  int i, j;

  /* the K&R method - more portable than sprintf with %lld */
  i = 0;
  do {
    tmp[i++] = n % 10 + '0';
  } while((n /= 10) > 0);
  tmp[i] = '\0';

  /* reverse and add thousand separators */
  j = 0;
  while(i--) {
    dat[j++] = tmp[i];
    if(i != 0 && i%3 == 0)
      dat[j++] = thou_sep;
  }
  dat[j] = '\0';

  return dat;
}


char *fmtmode(unsigned short mode) {
  static char buf[11];
  unsigned short ft = mode & S_IFMT;
  buf[0] = ft == S_IFDIR  ? 'd'
         : ft == S_IFREG  ? '-'
         : ft == S_IFLNK  ? 'l'
         : ft == S_IFIFO  ? 'p'
         : ft == S_IFSOCK ? 's'
         : ft == S_IFCHR  ? 'c'
         : ft == S_IFBLK  ? 'b' : '?';
  buf[1] = mode & 0400 ? 'r' : '-';
  buf[2] = mode & 0200 ? 'w' : '-';
  buf[3] = mode &04000 ? 's' : mode & 0100 ? 'x' : '-';
  buf[4] = mode & 0040 ? 'r' : '-';
  buf[5] = mode & 0020 ? 'w' : '-';
  buf[6] = mode &02000 ? 's' : mode & 0010 ? 'x' : '-';
  buf[7] = mode & 0004 ? 'r' : '-';
  buf[8] = mode & 0002 ? 'w' : '-';
  buf[9] = mode &01000 ? (S_ISDIR(mode) ? 't' : 'T') : mode & 0001 ? 'x' : '-';
  buf[10] = 0;
  return buf;
}


void read_locale(void) {
#ifdef HAVE_LOCALE_H
  char *locale_thou_sep;
#endif
  thou_sep = '.';
#ifdef HAVE_LOCALE_H
  setlocale(LC_ALL, "");
  locale_thou_sep = localeconv()->thousands_sep;
  if(locale_thou_sep && 1 == strlen(locale_thou_sep))
    thou_sep = locale_thou_sep[0];
#endif
}


int ncresize(int minrows, int mincols) {
  int ch;

  getmaxyx(stdscr, winrows, wincols);
  while((minrows && winrows < minrows) || (mincols && wincols < mincols)) {
    erase();
    mvaddstr(0, 0, "Warning: terminal too small,");
    mvaddstr(1, 1, "please either resize your terminal,");
    mvaddstr(2, 1, "press i to ignore, or press q to quit.");
    refresh();
    nodelay(stdscr, 0);
    ch = getch();
    getmaxyx(stdscr, winrows, wincols);
    if(ch == 'q') {
      erase();
      refresh();
      endwin();
      exit(0);
    }
    if(ch == 'i')
      return 1;
  }
  erase();
  return 0;
}


void nccreate(int height, int width, const char *title) {
  int i;

  uic_set(UIC_DEFAULT);
  subwinr = winrows/2-height/2;
  subwinc = wincols/2-width/2;

  /* clear window */
  for(i=0; i<height; i++)
    mvhline(subwinr+i, subwinc, ' ', width);

  /* box() only works around curses windows, so create our own */
  move(subwinr, subwinc);
  addch(ACS_ULCORNER);
  for(i=0; i<width-2; i++)
    addch(ACS_HLINE);
  addch(ACS_URCORNER);

  move(subwinr+height-1, subwinc);
  addch(ACS_LLCORNER);
  for(i=0; i<width-2; i++)
    addch(ACS_HLINE);
  addch(ACS_LRCORNER);

  mvvline(subwinr+1, subwinc, ACS_VLINE, height-2);
  mvvline(subwinr+1, subwinc+width-1, ACS_VLINE, height-2);

  /* title */
  uic_set(UIC_BOX_TITLE);
  mvaddstr(subwinr, subwinc+4, title);
  uic_set(UIC_DEFAULT);
}


void ncprint(int r, int c, const char *fmt, ...) {
  va_list arg;
  va_start(arg, fmt);
  move(subwinr+r, subwinc+c);
  vw_printw(stdscr, fmt, arg);
  va_end(arg);
}


void nctab(int c, int sel, int num, const char *str) {
  uic_set(sel ? UIC_KEY_HD : UIC_KEY);
  ncprint(0, c, "%d", num);
  uic_set(sel ? UIC_HD : UIC_DEFAULT);
  addch(':');
  addstr(str);
  uic_set(UIC_DEFAULT);
}


static int colors[UIC_NONE + 1];
static int lastcolor = 0;

struct base_attr {
  short fg, bg;
  int attr;
};

struct style_attr {
  short fg, bg;
  int attr;
  const struct base_attr *fallback;
};

struct style_def {
  struct style_attr off;
  struct style_attr dark;
  struct style_attr darkbg;
  struct style_attr dark2026;
};

static const struct style_def styles[UIC_NONE] = {
  [UIC_DEFAULT] = {
    .off      = { -1,            -1,          0                },
    .dark     = { -1,            -1,          0                },
    .darkbg   = { COLOR_WHITE,   COLOR_BLACK, 0                },
    .dark2026 = { -1,            -1,          0                } },
  [UIC_BOX_TITLE] = {
    .off      = { -1,            -1,          A_BOLD           },
    .dark     = { COLOR_BLUE,    -1,          A_BOLD           },
    .darkbg   = { COLOR_BLUE,    COLOR_BLACK, A_BOLD           },
    .dark2026 = { 111,           -1,          A_BOLD, &(const struct base_attr){ COLOR_BLUE,    -1,          A_BOLD } } },
  [UIC_HD] = {
    .off      = { -1,            -1,          A_REVERSE        },
    .dark     = { COLOR_BLACK,   COLOR_CYAN,  0                },
    .darkbg   = { COLOR_BLACK,   COLOR_CYAN,  0                },
    .dark2026 = { 254,           236,         0,      &(const struct base_attr){ COLOR_BLACK,   COLOR_CYAN,  0      } } },
  [UIC_SEL] = {
    .off      = { -1,            -1,          A_REVERSE        },
    .dark     = { COLOR_WHITE,   COLOR_GREEN, A_BOLD           },
    .darkbg   = { COLOR_WHITE,   COLOR_GREEN, A_BOLD           },
    .dark2026 = { 255,           23,          A_BOLD, &(const struct base_attr){ COLOR_WHITE,   COLOR_GREEN, A_BOLD } } },
  [UIC_NUM] = {
    .off      = { -1,            -1,          0                },
    .dark     = { COLOR_YELLOW,  -1,          A_BOLD           },
    .darkbg   = { COLOR_YELLOW,  COLOR_BLACK, A_BOLD           },
    .dark2026 = { 221,           -1,          A_BOLD, &(const struct base_attr){ COLOR_YELLOW,  -1,          A_BOLD } } },
  [UIC_NUM_HD] = {
    .off      = { -1,            -1,          A_REVERSE        },
    .dark     = { COLOR_YELLOW,  COLOR_CYAN,  A_BOLD           },
    .darkbg   = { COLOR_YELLOW,  COLOR_CYAN,  A_BOLD           },
    .dark2026 = { 221,           236,         A_BOLD, &(const struct base_attr){ COLOR_BLACK,   COLOR_CYAN,  A_BOLD } } },
  [UIC_NUM_SEL] = {
    .off      = { -1,            -1,          A_REVERSE        },
    .dark     = { COLOR_YELLOW,  COLOR_GREEN, A_BOLD           },
    .darkbg   = { COLOR_YELLOW,  COLOR_GREEN, A_BOLD           },
    .dark2026 = { 222,           23,          A_BOLD, &(const struct base_attr){ COLOR_WHITE,   COLOR_GREEN, A_BOLD } } },
  [UIC_KEY] = {
    .off      = { -1,            -1,          A_BOLD           },
    .dark     = { COLOR_YELLOW,  -1,          A_BOLD           },
    .darkbg   = { COLOR_YELLOW,  COLOR_BLACK, A_BOLD           },
    .dark2026 = { 221,           -1,          A_BOLD, &(const struct base_attr){ COLOR_YELLOW,  -1,          A_BOLD } } },
  [UIC_KEY_HD] = {
    .off      = { -1,            -1,          A_BOLD|A_REVERSE },
    .dark     = { COLOR_YELLOW,  COLOR_CYAN,  A_BOLD           },
    .darkbg   = { COLOR_YELLOW,  COLOR_CYAN,  A_BOLD           },
    .dark2026 = { 221,           236,         A_BOLD, &(const struct base_attr){ COLOR_BLACK,   COLOR_CYAN,  A_BOLD } } },
  [UIC_DIR] = {
    .off      = { -1,            -1,          0                },
    .dark     = { COLOR_BLUE,    -1,          A_BOLD           },
    .darkbg   = { COLOR_BLUE,    COLOR_BLACK, A_BOLD           },
    .dark2026 = { 111,           -1,          A_BOLD, &(const struct base_attr){ COLOR_BLUE,    -1,          A_BOLD } } },
  [UIC_DIR_SEL] = {
    .off      = { -1,            -1,          A_REVERSE        },
    .dark     = { COLOR_BLUE,    COLOR_GREEN, A_BOLD           },
    .darkbg   = { COLOR_BLUE,    COLOR_GREEN, A_BOLD           },
    .dark2026 = { 153,           23,          A_BOLD, &(const struct base_attr){ COLOR_WHITE,   COLOR_GREEN, A_BOLD } } },
  [UIC_FLAG] = {
    .off      = { -1,            -1,          0                },
    .dark     = { COLOR_RED,     -1,          0                },
    .darkbg   = { COLOR_RED,     COLOR_BLACK, 0                },
    .dark2026 = { 203,           -1,          0,      &(const struct base_attr){ COLOR_RED,     -1,          0      } } },
  [UIC_FLAG_SEL] = {
    .off      = { -1,            -1,          A_REVERSE        },
    .dark     = { COLOR_RED,     COLOR_GREEN, 0                },
    .darkbg   = { COLOR_RED,     COLOR_GREEN, 0                },
    .dark2026 = { 210,           23,          A_BOLD, &(const struct base_attr){ COLOR_WHITE,   COLOR_GREEN, A_BOLD } } },
  [UIC_GRAPH] = {
    .off      = { -1,            -1,          0                },
    .dark     = { COLOR_MAGENTA, -1,          0                },
    .darkbg   = { COLOR_MAGENTA, COLOR_BLACK, 0                },
    .dark2026 = { 141,           -1,          0,      &(const struct base_attr){ COLOR_MAGENTA, -1,          0      } } },
  [UIC_GRAPH_SEL] = {
    .off      = { -1,            -1,          A_REVERSE        },
    .dark     = { COLOR_MAGENTA, COLOR_GREEN, 0                },
    .darkbg   = { COLOR_MAGENTA, COLOR_GREEN, 0                },
    .dark2026 = { 183,           23,          0,      &(const struct base_attr){ COLOR_WHITE,   COLOR_GREEN, 0      } } }
};

static struct base_attr style_resolve(const struct style_attr *s, int use_256) {
  if(!use_256 && s->fallback)
    return *s->fallback;
  return (struct base_attr){ s->fg, s->bg, s->attr };
}

void uic_init(void) {
  int i, use_256;

  start_color();
  use_default_colors();
  use_256 = COLORS >= 256;
  for(i=0; i<UIC_NONE; i++) {
    const struct style_attr *s =
      uic_theme == 1 ? &styles[i].dark :
      uic_theme == 2 ? &styles[i].darkbg :
      uic_theme == 3 ? &styles[i].dark2026 : &styles[i].off;
    struct base_attr a = style_resolve(s, use_256);
    init_pair(i+1, a.fg, a.bg);
    colors[i] = a.attr | COLOR_PAIR(i+1);
  }
}

void uic_set(enum ui_coltype c) {
  attroff(lastcolor);
  lastcolor = colors[(int)c];
  attron(lastcolor);
}



/* removes item from the hlnk circular linked list and size counts of the parents */
static void freedir_hlnk(struct dir *d) {
  struct dir *t, *par, *pt;
  int i;

  if(!(d->flags & FF_HLNKC))
    return;

  /* remove size from parents.
   * This works the same as with adding: only the parents in which THIS is the
   * only occurrence of the hard link will be modified, if the same file still
   * exists within the parent it shouldn't get removed from the count.
   * XXX: Same note as for dir_mem.c / hlink_check():
   *      this is probably not the most efficient algorithm */
  for(i=1,par=d->parent; i&&par; par=par->parent) {
    if(d->hlnk)
      for(t=d->hlnk; i&&t!=d; t=t->hlnk)
        for(pt=t->parent; i&&pt; pt=pt->parent)
          if(pt==par)
            i=0;
    if(i) {
      par->size = adds64(par->size, -d->size);
      par->asize = adds64(par->size, -d->asize);
    }
  }

  /* remove from hlnk */
  if(d->hlnk) {
    for(t=d->hlnk; t->hlnk!=d; t=t->hlnk)
      ;
    t->hlnk = d->hlnk;
  }
}


static void freedir_rec(struct dir *dr) {
  struct dir *tmp, *tmp2;
  tmp2 = dr;
  while((tmp = tmp2) != NULL) {
    freedir_hlnk(tmp);
    /* remove item */
    if(tmp->sub) freedir_rec(tmp->sub);
    tmp2 = tmp->next;
    free(tmp);
  }
}


void freedir(struct dir *dr) {
  if(!dr)
    return;

  /* free dr->sub recursively */
  if(dr->sub)
    freedir_rec(dr->sub);

  /* update references */
  if(dr->parent && dr->parent->sub == dr)
    dr->parent->sub = dr->next;
  if(dr->prev)
    dr->prev->next = dr->next;
  if(dr->next)
    dr->next->prev = dr->prev;

  freedir_hlnk(dr);

  /* update sizes of parent directories if this isn't a hard link.
   * If this is a hard link, freedir_hlnk() would have done so already
   *
   * mtime is 0 here because recalculating the maximum at every parent
   * dir is expensive, but might be good feature to add later if desired */
  addparentstats(dr->parent, dr->flags & FF_HLNKC ? 0 : -dr->size, dr->flags & FF_HLNKC ? 0 : -dr->asize, 0, -(dr->items+1));

  free(dr);
}


const char *getpath(struct dir *cur) {
  static char *dat;
  static int datl = 0;
  struct dir *d, **list;
  int c, i;

  if(!cur->name[0])
    return "/";

  c = i = 1;
  for(d=cur; d!=NULL; d=d->parent) {
    i += strlen(d->name)+1;
    c++;
  }

  if(datl == 0) {
    datl = i;
    dat = xmalloc(i);
  } else if(datl < i) {
    datl = i;
    dat = xrealloc(dat, i);
  }
  list = xmalloc(c*sizeof(struct dir *));

  c = 0;
  for(d=cur; d!=NULL; d=d->parent)
    list[c++] = d;

  dat[0] = '\0';
  while(c--) {
    if(list[c]->parent)
      strcat(dat, "/");
    strcat(dat, list[c]->name);
  }
  free(list);
  return dat;
}


struct dir *getroot(struct dir *d) {
  while(d && d->parent)
    d = d->parent;
  return d;
}


void addparentstats(struct dir *d, int64_t size, int64_t asize, uint64_t mtime, int items) {
  struct dir_ext *e;
  while(d) {
    d->size = adds64(d->size, size);
    d->asize = adds64(d->asize, asize);
    d->items += items;
    if (d->flags & FF_EXT) {
      e = dir_ext_ptr(d);
      e->mtime = (e->mtime > mtime) ? e->mtime : mtime;
    }
    d = d->parent;
  }
}


/* Apparently we can just resume drawing after endwin() and ncurses will pick
 * up where it left. Probably not very portable...  */
#define oom_msg "\nOut of memory, press enter to try again or Ctrl-C to give up.\n"
#define wrap_oom(f) \
  void *ptr;\
  char buf[128];\
  while((ptr = f) == NULL) {\
    close_nc();\
    write(2, oom_msg, sizeof(oom_msg)-1);\
    read(0, buf, sizeof(buf));\
  }\
  return ptr;

void *xmalloc(size_t size) { wrap_oom(malloc(size)) }
void *xcalloc(size_t n, size_t size) { wrap_oom(calloc(n, size)) }
void *xrealloc(void *mem, size_t size) { wrap_oom(realloc(mem, size)) }

char *xstrdup(const char *str) {
  char *r = xmalloc(strlen(str)+1);
  strcpy(r, str);
  return r;
}


/* Expands '~' and '~user' */
char *expanduser(const char *path) {
  size_t len, size;
  struct passwd *pwd;
  char *home = NULL, *tmp;

  if(path[0] != '~') return xstrdup(path);
  len = strcspn(path+1, "/");

  if(len == 0) {
    home = getenv("HOME");
    if(!home) {
      pwd = getpwuid(getuid());
      if(pwd) home = pwd->pw_dir;
    }
  } else {
    tmp = xmalloc(len+1);
    memcpy(tmp, path+1, len);
    tmp[len] = 0;
    pwd = getpwnam(tmp);
    free(tmp);
    if(pwd) home = pwd->pw_dir;
  }
  if(!home) return xstrdup(path);

  size = strlen(home);
  while(size > 0 && home[size-1] == '/') size--;
  home[size] = 0;

  if(size == 0 && path[len+1] == 0) return xstrdup("/");

  size += strlen(path) - len;
  tmp = xmalloc(size);
  snprintf(tmp, size, "%s%s", home, path+1+len);
  return tmp;
}
