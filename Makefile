# SPDX-FileCopyrightText: Yorhel <projects@yorhel.nl>
# SPDX-License-Identifier: MIT

# Optional semi-standard Makefile with some handy tools.
# Ncdu itself can be built with just the zig build system.

ZIG ?= zig

PREFIX ?= /usr/local
BINDIR ?= ${PREFIX}/bin
MANDIR ?= ${PREFIX}/share/man/man1
ZIG_FLAGS ?= --release=fast -Dstrip -fsys=zstd

NCDU_VERSION=$(shell grep '\.version' build.zig.zon | cut -d'"' -f2)

.PHONY: build test
build: release

release:
	$(ZIG) build ${ZIG_FLAGS}

debug:
	$(ZIG) build

clean:
	rm -rf .zig-cache zig-pkg zig-out

install: install-bin install-doc

install-bin: release
	mkdir -p ${BINDIR}
	install -m0755 zig-out/bin/ncdu ${BINDIR}/

install-doc:
	mkdir -p ${MANDIR}
	install -m0644 ncdu.1 ${MANDIR}/

uninstall: uninstall-bin uninstall-doc

# XXX: Ideally, these would also remove the directories created by 'install' if they are empty.
uninstall-bin:
	rm -f ${BINDIR}/ncdu

uninstall-doc:
	rm -f ${MANDIR}/ncdu.1

dist:
	rm -f ncdu-${NCDU_VERSION}.tar.gz
	mkdir ncdu-${NCDU_VERSION}
	for f in `git ls-files | grep -v ^\.gitignore`; do mkdir -p ncdu-${NCDU_VERSION}/`dirname $$f`; ln -s "`pwd`/$$f" ncdu-${NCDU_VERSION}/$$f; done
	tar -cophzf ncdu-${NCDU_VERSION}.tar.gz --sort=name ncdu-${NCDU_VERSION}
	rm -rf ncdu-${NCDU_VERSION}


# EDIT UPDATE TODO WIP: regressed in this branch, does not work,
# fix in this branch later by moving as much as possible to build.zig!
# ASSUMPTION:
# - the ncurses source tree has been extracted into ncurses/
# - the zstd source tree has been extracted into zstd/
# Would be nicer to do all this with the Zig build system, but no way am I
# going to write build.zig's for these projects.
static-%.tar.gz:
	mkdir -p static-$*/nc static-$*/inst/pkg
	cd static-$*/nc && ../../ncurses/configure --prefix="`pwd`/../inst"\
		--without-cxx --without-cxx-binding --without-ada --without-manpages --without-progs\
		--without-tests --disable-pc-files --without-pkg-config --without-shared --without-debug\
		--without-gpm --without-sysmouse --enable-widec --with-default-terminfo-dir=/usr/share/terminfo\
		--with-terminfo-dirs=/usr/share/terminfo:/lib/terminfo:/usr/local/share/terminfo\
		--with-fallbacks="screen linux vt100 xterm xterm-256color" --host=$*\
		CC="${ZIG} cc --target=$*"\
		LD="${ZIG} cc --target=$*"\
		AR="${ZIG} ar" RANLIB="${ZIG} ranlib"\
		CPPFLAGS=-D_GNU_SOURCE && make -j8
	@# zig-build - cleaner approach but doesn't work, results in a dynamically linked binary.
	@#cd static-$* && PKG_CONFIG_LIBDIR="`pwd`/inst/pkg" zig build -Dtarget=$*
	@#	--build-file ../build.zig --search-prefix inst/ --cache-dir zig -Drelease-fast=true
	@# Alternative approach, bypassing zig-build
	cd static-$* && ${ZIG} build-exe -target $*\
		-Inc/include -Izstd -lc nc/lib/libncursesw.a zstd/libzstd.a\
		--cache-dir zig-cache -static -fstrip -O ReleaseFast ../src/main.zig
	@# My system's strip can't deal with arm binaries and zig doesn't wrap a strip alternative.
	@# Whatever, just let it error for those.
	strip -R .eh_frame -R .eh_frame_hdr static-$*/main || true
	cd static-$* && mv main ncdu && tar -czf ../static-$*.tar.gz ncdu
	rm -rf static-$*

static-linux-x86_64: static-x86_64-linux-musl.tar.gz
	mv $< ncdu-${NCDU_VERSION}-linux-x86_64.tar.gz

static-linux-x86: static-x86-linux-musl.tar.gz
	mv $< ncdu-${NCDU_VERSION}-linux-x86.tar.gz

static-linux-aarch64: static-aarch64-linux-musl.tar.gz
	mv $< ncdu-${NCDU_VERSION}-linux-aarch64.tar.gz

static-linux-arm: static-arm-linux-musleabi.tar.gz
	mv $< ncdu-${NCDU_VERSION}-linux-arm.tar.gz

static:\
	static-linux-x86_64 \
	static-linux-x86 \
	static-linux-aarch64 \
	static-linux-arm

test:
	zig build test
	mandoc -T lint ncdu.1
	reuse lint
