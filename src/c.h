// SPDX-FileCopyrightText: Yorhel <projects@yorhel.nl>
// SPDX-License-Identifier: MIT

#define _XOPEN_SOURCE 700 // for wcwidth()
#include <fnmatch.h>      // fnmatch()
#include <locale.h>       // setlocale() and localeconv()
#include <pwd.h>          // getpwnam(), getpwuid()
#include <stdio.h>        // fopen(), used to initialize ncurses
#include <string.h>       // strerror()
#include <sys/types.h>    // struct passwd
#include <time.h>         // strftime()
#include <unistd.h>       // getuid()
#include <wchar.h>        // wcwidth()
#if defined(__linux__)
#include <sys/vfs.h> // statfs()
#endif
#include <curses.h>
#include <zstd.h>
