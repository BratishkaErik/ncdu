<!--
SPDX-FileCopyrightText: Yorhel <projects@yorhel.nl>
SPDX-License-Identifier: MIT
-->

# ncdu-zig

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

Ncdu is a disk usage analyzer with an ncurses interface. It is designed to find
space hogs on a remote server where you don't have an entire graphical setup
available, but it is a useful tool even on regular desktop systems. Ncdu aims
to be fast, simple and easy to use, and should be able to run in any minimal
POSIX-like environment with ncurses installed.

See the [ncdu 2 release announcement](https://dev.yorhel.nl/doc/ncdu2) for
information about the differences between this Zig implementation (2.x) and the
C version (1.x).

## Build Requirements

- Zig 0.16
- Some sort of POSIX-like OS

## Library Dependencies

- ncurses (must be installed on your system)
- libzstd (can use Zig-packaged version, or system version)

## Install

You can use the Zig build system if you're familiar with that.
By default, Zig will fetch and build `libzstd` for you.
If you want to use your system libzstd package instead, pass the `-fsys=zstd` flag:

```shell
zig build -fsys=zstd
```

Alternatively, you can use system mode to automatically use system package:

```shell
zig build --system "path/to/downloaded/zig-packages/"
```

---

There's also a handy Makefile that supports the typical targets. By default,
the Makefile is configured to pass following flags to Zig build:
`--release=fast -Dstrip -fsys=zstd`. You can override it with `ZIG_FLAGS`.

```shell
make
sudo make install PREFIX=/usr
```
