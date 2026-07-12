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

#include "global.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <sys/time.h>


int pstate;
int can_delete = -1;
int can_shell = -1;
int can_refresh = -1;
long update_delay = 100;
int cachedir_tags = 0;
int extended_info = 0;
int follow_symlinks = 0;
int follow_firmlinks = 1;
int confirm_quit = 0;
int si = 0;
int show_as = 0;
int graph = 1;
int graph_style = 0;
int show_items = 0;
int show_mtime = 0;

static int min_rows = 17, min_cols = 60;
static int ncurses_init = 0;
static int ncurses_tty = 0; /* Explicitly open /dev/tty instead of using stdio */
static long lastupdate = 999;


static void screen_draw(void) {
  switch(pstate) {
    case ST_CALC:   dir_draw();    break;
    case ST_BROWSE: browse_draw(); break;
    case ST_HELP:   help_draw();   break;
    case ST_SHELL:  shell_draw();  break;
    case ST_DEL:    delete_draw(); break;
    case ST_QUIT:   quit_draw();   break;
  }
}


/* wait:
 *  -1: non-blocking, always draw screen
 *   0: blocking wait for input and always draw screen
 *   1: non-blocking, draw screen only if a configured delay has passed or after keypress
 */
int input_handle(int wait) {
  int ch;
  struct timeval tv;

  if(wait != 1)
    screen_draw();
  else {
    gettimeofday(&tv, NULL);
    tv.tv_usec = (1000*(tv.tv_sec % 1000) + (tv.tv_usec / 1000)) / update_delay;
    if(lastupdate != tv.tv_usec) {
      screen_draw();
      lastupdate = tv.tv_usec;
    }
  }

  /* No actual input handling is done if ncurses hasn't been initialized yet. */
  if(!ncurses_init)
    return wait == 0 ? 1 : 0;

  nodelay(stdscr, wait?1:0);
  errno = 0;
  while((ch = getch()) != ERR) {
    if(ch == KEY_RESIZE) {
      if(ncresize(min_rows, min_cols))
        min_rows = min_cols = 0;
      /* ncresize() may change nodelay state, make sure to revert it. */
      nodelay(stdscr, wait?1:0);
      screen_draw();
      continue;
    }
    switch(pstate) {
      case ST_CALC:   return dir_key(ch);
      case ST_BROWSE: return browse_key(ch);
      case ST_HELP:   return help_key(ch);
      case ST_DEL:    return delete_key(ch);
      case ST_QUIT:   return quit_key(ch);
    }
    screen_draw();
  }
  if(errno == EPIPE || errno == EBADF || errno == EIO)
      return 1;
  return 0;
}


/* This is a backport of the argument parser in the Zig version.
 * Minor differences in that this implementation can modify argv in-place and has a slightly different API. */
static struct argparser {
  int argc;
  char **argv;
  char *shortopt;
  char *last;
  char *last_arg;
  char shortbuf[2];
  char argsep;
  char ignerror;
} argparser_state;

static char *argparser_pop(struct argparser *p) {
  char *a;
  if(p->argc == 0) return NULL;
  a = *p->argv;
  p->argv++;
  p->argc--;
  return a;
}

static int argparser_shortopt(struct argparser *p, char *buf) {
  p->shortbuf[0] = '-';
  p->shortbuf[1] = *buf;
  p->shortopt = buf[1] ? buf+1 : NULL;
  p->last = p->shortbuf;
  return 1;
}

/* Returns -1 on error (only when ignerror), 0 when done, 1 if there's an option, 2 if there's a positional argument. */
static int argparser_next(struct argparser *p) {
  if(p->last_arg) { if(p->ignerror) return -1; die("Option '%s' does not expect an argument.\n", p->last); }
  if(p->shortopt) return argparser_shortopt(p, p->shortopt);
  p->last = argparser_pop(p);
  if(!p->last) return 0;
  if(p->argsep || !*p->last || *p->last != '-') return 2;
  if(!p->last[1]) { if(p->ignerror) return -1; die("Invalid option '-'.\n"); }
  if(p->last[1] == '-' && !p->last[2]) { /* '--' argument separator */
    p->argsep = 1;
    return argparser_next(p);
  }
  if(p->last[1] == '-') { /* long option */
    p->last_arg = strchr(p->last, '=');
    if(p->last_arg) {
      *p->last_arg = 0;
      p->last_arg++;
    }
    return 1;
  }
  /* otherwise: short option */
  return argparser_shortopt(p, p->last+1);
}

static char *argparser_arg(struct argparser *p) {
  char *tmp;
  if(p->shortopt) {
    tmp = p->shortopt;
    p->shortopt = NULL;
    return tmp;
  }
  if(p->last_arg) {
    tmp = p->last_arg;
    p->last_arg = NULL;
    return tmp;
  }
  tmp = argparser_pop(p);
  if(!tmp) { if(p->ignerror) return NULL; die("Option '%s' requires an argument.\n", p->last); }
  return tmp;
}

#define OPT(_s) (strcmp(argparser_state.last, (_s)) == 0)
#define ARG (argparser_arg(&argparser_state))

static int arg_option(int infile) {
  char *arg, *tmp;
  if(OPT("-q") || OPT("--slow-ui-updates")) update_delay = 2000;
  else if(OPT("--fast-ui-updates")) update_delay = 100;
  else if(OPT("-x") || OPT("--one-file-system")) dir_scan_smfs = 1;
  else if(OPT("--cross-file-system")) dir_scan_smfs = 0;
  else if(OPT("-e") || OPT("--extended")) extended_info = 1;
  else if(OPT("--no-extended")) extended_info = 0;
  else if(OPT("-r") && !can_delete) can_shell = 0;
  else if(OPT("-r")) can_delete = 0;
  else if(OPT("--enable-shell")) can_shell = 1;
  else if(OPT("--disable-shell")) can_shell = 0;
  else if(OPT("--enable-delete")) can_delete = 1;
  else if(OPT("--disable-delete")) can_delete = 0;
  else if(OPT("--enable-refresh")) can_refresh = 1;
  else if(OPT("--disable-refresh")) can_refresh = 0;
  else if(OPT("--show-hidden")) dirlist_hidden = 0;
  else if(OPT("--hide-hidden")) dirlist_hidden = 1;
  else if(OPT("--show-itemcount")) show_items = 1;
  else if(OPT("--hide-itemcount")) show_items = 0;
  else if(OPT("--show-mtime")) show_mtime = 1;
  else if(OPT("--hide-mtime")) show_mtime = 0;
  else if(OPT("--show-graph")) graph |= 1;
  else if(OPT("--hide-graph")) graph &= 2;
  else if(OPT("--show-percent")) graph |= 2;
  else if(OPT("--hide-percent")) graph &= 1;
  else if(OPT("--group-directories-first")) dirlist_sort_df = 1;
  else if(OPT("--no-group-directories-first")) dirlist_sort_df = 0;
  else if(OPT("--enable-natsort")) dirlist_natsort = 1;
  else if(OPT("--disable-natsort")) dirlist_natsort = 0;
  else if(OPT("--graph-style")) {
    arg = ARG;
    if (!arg) return 1;
    else if (strcmp(arg, "hash") == 0) graph_style = 0;
    else if (strcmp(arg, "half-block") == 0) graph_style = 1;
    else if (strcmp(arg, "eighth-block") == 0 || strcmp(arg, "eigth-block") == 0) graph_style = 2;
    else if (!argparser_state.ignerror) die("Unknown --graph-style option: %s.\n", arg);
  } else if(OPT("--sort")) {
    arg = ARG;
    if (!arg) return 1;
    tmp = strrchr(arg, '-');
    if(tmp && (strcmp(tmp, "-asc") == 0 || strcmp(tmp, "-desc") == 0)) *tmp = 0;

    if(strcmp(arg, "name") == 0) {
      dirlist_sort_col = DL_COL_NAME;
      dirlist_sort_desc = 0;
    } else if(strcmp(arg, "disk-usage") == 0) {
      dirlist_sort_col = DL_COL_SIZE;
      dirlist_sort_desc = 1;
    } else if(strcmp(arg, "apparent-size") == 0) {
      dirlist_sort_col = DL_COL_ASIZE;
      dirlist_sort_desc = 1;
    } else if(strcmp(arg, "itemcount") == 0) {
      dirlist_sort_col = DL_COL_ITEMS;
      dirlist_sort_desc = 1;
    } else if(strcmp(arg, "mtime") == 0) {
      dirlist_sort_col = DL_COL_MTIME;
      dirlist_sort_desc = 0;
    } else if(argparser_state.ignerror) return 1;
    else die("Invalid argument to --sort: '%s'.\n", arg);

    if(tmp && !*tmp) dirlist_sort_desc = tmp[1] == 'd';
  } else if(OPT("--apparent-size")) show_as = 1;
  else if(OPT("--disk-usage")) show_as = 0;
  else if(OPT("-0")) dir_ui = 0;
  else if(OPT("-1")) dir_ui = 1;
  else if(OPT("-2")) dir_ui = 2;
  else if(OPT("--si")) si = 1;
  else if(OPT("--no-si")) si = 0;
  else if(OPT("-L") || OPT("--follow-symlinks")) follow_symlinks = 1;
  else if(OPT("--no-follow-symlinks")) follow_symlinks = 0;
  else if(OPT("--exclude")) {
    arg = ARG;
    if(!arg) return 1;
    if(infile) arg = expanduser(arg);
    exclude_add(arg);
    if(infile) free(arg);
  } else if(OPT("-X") || OPT("--exclude-from")) {
    arg = ARG;
    if(!arg) return 1;
    if(infile) arg = expanduser(arg);
    if(exclude_addfile(arg)) { if (argparser_state.ignerror) return 1; die("Can't open %s: %s\n", arg, strerror(errno)); }
    if(infile) free(arg);
  } else if(OPT("--exclude-caches")) cachedir_tags = 1;
  else if(OPT("--include-caches")) cachedir_tags = 0;
  else if(OPT("--exclude-kernfs")) exclude_kernfs = 1;
  else if(OPT("--include-kernfs")) exclude_kernfs = 0;
  else if(OPT("--follow-firmlinks")) follow_firmlinks = 1;
  else if(OPT("--exclude-firmlinks")) follow_firmlinks = 0;
  else if(OPT("--confirm-quit")) confirm_quit = 1;
  else if(OPT("--no-confirm-quit")) confirm_quit = 0;
  else if(OPT("--confirm-delete")) delete_confirm = 1;
  else if(OPT("--no-confirm-delete")) delete_confirm = 0;
  else if(OPT("--color")) {
    arg = ARG;
    if (!arg) return 1;
    else if(strcmp(arg, "off") == 0) uic_theme = 0;
    else if(strcmp(arg, "dark") == 0) uic_theme = 1;
    else if(strcmp(arg, "dark-bg") == 0) uic_theme = 2;
    else if (!argparser_state.ignerror) die("Unknown --color option: %s\n", arg);
  } else return 0;
  return 1;
}

static void arg_help(void) {
  printf(
  "ncdu <options> <directory>\n"
  "\n"
  "Mode selection:\n"
  "  -h, --help                 This help message\n"
  "  -v, -V, --version          Print version\n"
  "  -f FILE                    Import scanned directory from FILE\n"
  "  -o FILE                    Export scanned directory to FILE in JSON format\n"
  "  -e, --extended             Enable extended information\n"
  "  --ignore-config            Don't load config files\n"
  "\n"
  "Scan options:\n"
  "  -x, --one-file-system      Stay on the same filesystem\n"
  "  --exclude PATTERN          Exclude files that match PATTERN\n"
  "  -X, --exclude-from FILE    Exclude files that match any pattern in FILE\n"
  "  --exclude-caches           Exclude directories containing CACHEDIR.TAG\n"
  "  -L, --follow-symlinks      Follow symbolic links (excluding directories)\n"
#if HAVE_LINUX_MAGIC_H && HAVE_SYS_STATFS_H && HAVE_STATFS
  "  --exclude-kernfs           Exclude Linux pseudo filesystems (procfs,sysfs,cgroup,...)\n"
#endif
#if HAVE_SYS_ATTR_H && HAVE_GETATTRLIST && HAVE_DECL_ATTR_CMNEXT_NOFIRMLINKPATH
  "  --exclude-firmlinks        Exclude firmlinks on macOS\n"
#endif
  "\n"
  "Interface options:\n"
  "  -0, -1, -2                 UI to use when scanning (0=none,2=full ncurses)\n"
  "  -q, --slow-ui-updates      \"Quiet\" mode, refresh interval 2 seconds\n"
  "  --enable-shell             Enable/disable shell spawning feature\n"
  "  --enable-delete            Enable/disable file deletion feature\n"
  "  --enable-refresh           Enable/disable directory refresh feature\n"
  "  -r                         Read only (--disable-delete)\n"
  "  -rr                        Read only++ (--disable-delete & --disable-shell)\n"
  "  --si                       Use base 10 (SI) prefixes instead of base 2\n"
  "  --apparent-size            Show apparent size instead of disk usage by default\n"
  "  --hide-hidden              Hide \"hidden\" or excluded files by default\n"
  "  --show-itemcount           Show item count column by default\n"
  "  --show-mtime               Show mtime column by default (requires `-e`)\n"
  "  --show-graph               Show graph column by default\n"
  "  --show-percent             Show percent column by default\n"
  "  --graph-style STYLE        hash / half-block / eighth-block\n"
  "  --sort COLUMN-(asc/desc)   disk-usage / name / apparent-size / itemcount / mtime\n"
  "  --enable-natsort           Use natural order when sorting by name\n"
  "  --group-directories-first  Sort directories before files\n"
  "  --confirm-quit             Ask confirmation before quitting ncdu\n"
  "  --no-confirm-delete        Don't ask confirmation before deletion\n"
  "  --color SCHEME             off / dark / dark-bg\n"
  "\n"
  "Refer to `man ncdu` for more information.\n");
  exit(0);
}


static void config_read(const char *fn) {
  FILE *f;
  char buf[1024], *line, *tmp, *args[3];
  int r, len;

  if((f = fopen(fn, "r")) == NULL) {
    if(errno == ENOENT || errno == ENOTDIR) return;
    die("Error opening %s: %s.\nRun with --ignore-config to skip reading config files.\n", fn, strerror(errno));
  }

  while(fgets(buf, 1024, f) != NULL) {
    line = buf;
    while(*line == ' ' || *line == '\t') line++;
    len = strlen(line);
    while(len > 0 && (line[len-1] == ' ' || line[len-1] == '\t' || line[len-1] == '\r' || line[len-1] == '\n')) len -= 1;
    line[len] = 0;
    if(len == 0 || *line == '#') continue;

    memset(&argparser_state, 0, sizeof(struct argparser));
    argparser_state.argv = args;

    if (*line == '@') {
        argparser_state.ignerror = 1;
        line++;
        if (!*line || *line == '#') continue;
    }
    args[argparser_state.argc++] = line;

    for(tmp=line; *tmp && *tmp != ' ' && *tmp != '\t'; tmp++);
    while(*tmp && (*tmp == ' ' || *tmp == '\t')) {
      *tmp = 0;
      tmp++;
    }
    if(*tmp) args[argparser_state.argc++] = tmp;
    args[argparser_state.argc] = NULL;

    while((r = argparser_next(&argparser_state)) > 0) {
      if(r == 2 || !arg_option(1)) {
        if (argparser_state.ignerror) break;
        die("Unknown option in config file '%s': %s.\nRun with --ignore-config to skip reading config files.\n", fn, argparser_state.last);
      }
    }
  }
  if(ferror(f))
    die("Error reading from %s: %s\nRun with --ignore-config to skip reading config files.\n", fn, strerror(errno));
  fclose(f);
}


static void config_load(int argc, char **argv) {
  char *env, buf[1024];
  int r;

  for(r=0; r<argc; r++)
    if(strcmp(argv[r], "--ignore-config") == 0) return;

  config_read("/etc/ncdu.conf");

  if((env = getenv("XDG_CONFIG_HOME")) != NULL) {
    r = snprintf(buf, 1024, "%s/ncdu/config", env);
    if(r > 0 && r < 1024) config_read(buf);
  } else if((env = getenv("HOME")) != NULL) {
    r = snprintf(buf, 1024, "%s/.config/ncdu/config", env);
    if(r > 0 && r < 1024) config_read(buf);
  }
}


static void argv_parse(int argc, char **argv) {
  int r;
  char *export = NULL;
  char *import = NULL;
  char *dir = NULL;

  memset(&argparser_state, 0, sizeof(struct argparser));
  argparser_state.argv = argv;
  argparser_state.argc = argc;
  argparser_next(&argparser_state); /* skip program name */

  while((r = argparser_next(&argparser_state)) > 0) {
    if(r == 2) dir = argparser_state.last;
    else if(OPT("-v") || OPT("-V") || OPT("--version")) {
      printf("ncdu %s\n", PACKAGE_VERSION);
      exit(0);
    } else if(OPT("-h") || OPT("-?") || OPT("--help")) arg_help();
    else if(OPT("-o")) export = ARG;
    else if(OPT("-f")) import = ARG;
    else if(OPT("--ignore-config")) {}
    else if(!arg_option(0)) die("Unknown option '%s'.\n", argparser_state.last);
  }

#if !(HAVE_LINUX_MAGIC_H && HAVE_SYS_STATFS_H && HAVE_STATFS)
  if(exclude_kernfs) die("The --exclude-kernfs flag is currently only supported on Linux.\n");
#endif

  if(export) {
    if(dir_export_init(export)) die("Can't open %s: %s\n", export, strerror(errno));
    if(strcmp(export, "-") == 0) ncurses_tty = 1;
  } else
    dir_mem_init(NULL);

  if(import) {
    if(dir_import_init(import)) die("Can't open %s: %s\n", import, strerror(errno));
    if(strcmp(import, "-") == 0) ncurses_tty = 1;
  } else
    dir_scan_init(dir ? dir : ".");

  /* Use the single-line scan feedback by default when exporting to file, no
   * feedback when exporting to stdout. */
  if(dir_ui == -1)
    dir_ui = export && strcmp(export, "-") == 0 ? 0 : export ? 1 : 2;

  if(can_delete == -1)  can_delete  = import ? 0 : 1;
  if(can_shell == -1)   can_shell   = import ? 0 : 1;
  if(can_refresh == -1) can_refresh = import ? 0 : 1;
}


/* Initializes ncurses only when not done yet. */
static void init_nc(void) {
  int ok = 0;
  FILE *tty;
  SCREEN *term;

  if(ncurses_init)
    return;
  ncurses_init = 1;

  if(ncurses_tty) {
    tty = fopen("/dev/tty", "r+");
    if(!tty) die("Error opening /dev/tty: %s\n", strerror(errno));
    term = newterm(NULL, tty, tty);
    if(term)
      set_term(term);
    ok = !!term;
  } else {
    /* Make sure the user doesn't accidentally pipe in data to ncdu's standard
     * input without using "-f -". An annoying input sequence could result in
     * the deletion of your files, which we want to prevent at all costs. */
    if(!isatty(0)) die("Standard input is not a TTY. Did you mean to import a file using '-f -'?\n");
    ok = !!initscr();
  }

  if(!ok) die("Error while initializing ncurses.\n");

  uic_init();
  cbreak();
  noecho();
  curs_set(0);
  keypad(stdscr, TRUE);
  bkgd(COLOR_PAIR(UIC_DEFAULT+1));
  if(ncresize(min_rows, min_cols))
    min_rows = min_cols = 0;
}


void close_nc(void) {
  if(ncurses_init) {
    erase();
    refresh();
    endwin();
  }
}


int main(int argc, char **argv) {
  read_locale();
  config_load(argc, argv);
  argv_parse(argc, argv);

  if(dir_ui == 2)
    init_nc();

  while(1) {
    /* We may need to initialize/clean up the screen when switching from the
     * (sometimes non-ncurses) CALC state to something else. */
    if(pstate != ST_CALC) {
      if(dir_ui == 1)
        fputc('\n', stderr);
      init_nc();
    }

    if(pstate == ST_CALC) {
      if(dir_process()) {
        if(dir_ui == 1)
          fputc('\n', stderr);
        break;
      }
    } else if(pstate == ST_DEL)
      delete_process();
    else if(input_handle(0))
      break;
  }

  close_nc();
  exclude_clear();

  return 0;
}

