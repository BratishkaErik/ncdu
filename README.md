<!--
SPDX-FileCopyrightText: Yorhel <projects@yorhel.nl>
SPDX-License-Identifier: MIT
-->

# ncdu-zig

> [!IMPORTANT]
> **This is a Maintenance Fork!**
>
> Following the [passing of Yoran Heling (aka Yorhel)](https://vndb.org/t24787),
> the original author of Ncdu, this repository serves as a maintenance fork
> to keep his project alive.
>
> Read the [announcement post](https://ncdu.landless-city.net/blog/2026-07-12-maintenance-fork/)
> for more context.
>
> The new project homepage is available at [ncdu.landless-city.net](https://ncdu.landless-city.net).

## Description

Ncdu is a disk usage analyzer with an ncurses interface. It is designed to find
space hogs on a remote server where you don't have an entire graphical setup
available, but it is a useful tool even on regular desktop systems. Ncdu aims
to be fast, simple and easy to use, and should be able to run in any minimal
POSIX-like environment with ncurses installed.

See the [ncdu 2 release announcement](https://ncdu.landless-city.net/blog/2021-07-22-ncdu2/) for
information about the differences between this Zig implementation (2.x) and the
C version (1.x).

## Build Requirements

- Zig 0.17
- Some sort of POSIX-like OS

## Library Dependencies

- ncurses (with wide-character support)
- libzstd

Both libraries can be fetched and built automatically by Zig, or you can use
your system's installed versions.

## Install

You can use the Zig build system if you're familiar with that.
By default, Zig will fetch and build `ncurses` and `libzstd` for you.
If you want to use your system package instead, pass the `-fsys=[...]` flag.
You can mix and match these depending on what you have installed locally:

```shell
# Use system versions for both libraries
zig build -fsys=ncurses -fsys=zstd

# Use system ncurses, but let Zig fetch libzstd
zig build -fsys=ncurses -fno-sys=zstd
```

Alternatively, you can use system mode to automatically use system packages
for all dependencies:

```shell
zig build --system "path/to/downloaded/zig-packages/"
```

---

There's also a handy Makefile that supports the typical targets. By default,
the Makefile is configured to pass following flags to Zig build:
`--release=fast -Dstrip -fsys=zstd -fsys=ncurses`.
You can override it with `ZIG_FLAGS`.

```shell
make
sudo make install PREFIX=/usr
```
