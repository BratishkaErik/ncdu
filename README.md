# ncdu 1.23.0

## Project status

### Background

> [!IMPORTANT]
> **This is a Maintenance Fork!**
>
> Following the [passing of Yoran Heling (aka Yorhel)](https://vndb.org/t24787),
> the original author of Ncdu, this repository serves as a maintenance fork
> to keep his project alive.
>
> See the [original upstream discussion](https://code.blicky.net/yorhel/ncdu/issues/276)
> for more context.

### Current maintainers

*(Just me for now, but co-maintainers are very welcome)*

I, BratishkaErik, have some experience
[helping upstream](https://code.blicky.net/yorhel/ncdu/pulls?q=&type=pr&state=all&poster=387)
update `ncdu` to newer Zig versions, so I will *(for now)* focus on keeping
the Zig branch updated.

I need to properly refresh my C knowledge and skills before I can comfortably
maintain the C branch.

If someone more qualified is willing to step up and lead the maintenance,
I would be more than happy to assist and hand over this fork.

### Git branches layout

Original upstream branches are frozen to preserve and honor history.
Active development has moved to new branches:

* C version (1.x): `master` → `c-version`
* Zig version (2.x): `zig` → `zig-version`

### Future of the project

Regarding the hosting: I chose GitHub to avoid the risk of losing the project
if the upstream Forgejo instance goes down, and Codeberg can be quite laggy.

Future releases will be tagged and published from the new branches.
I will be writing new changelogs directly in GitHub Releases,
instead of updating `ChangeLog` file.

Please note that I do not have access to Yorhel's original PGP signing keys,
and even if I had, using them IMO would not be appropriate anyway.

I **won't** use my own personal PGP keys either, as tying verification
to a single person *again* just creates another single point of failure.
Instead, new releases will be signed and verified using Sigstore, with
GitHub Attestations.

---

## Description

ncdu (NCurses Disk Usage) is a curses-based version of
the well-known 'du', and provides a fast way to see what
directories are using your disk space.

## Requirements

In order to compile and install ncdu, you need to have
at least...

- a POSIX-compliant operating system (Linux, BSD, etc)
- curses libraries and header files

## Install

The usual:

```bash
./configure --prefix=/usr
make
make install
```

If you're building directly from the git repository, make sure you have
pkg-config and GNU autoconf/automake installed, then run 'autoreconf -i',
and you're ready to continue with the usual ./configure and make route.

## Copying

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
