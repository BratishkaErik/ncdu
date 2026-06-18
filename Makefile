# SPDX-FileCopyrightText: Yorhel <projects@yorhel.nl>
# SPDX-License-Identifier: MIT

# Optional semi-standard Makefile with some handy tools.
# Ncdu itself can be built with just the zig build system.

ZIG ?= zig

PREFIX ?= /usr/local
BINDIR ?= ${PREFIX}/bin
MANDIR ?= ${PREFIX}/share/man/man1
ZIG_FLAGS ?= --release=fast -Dstrip -fsys=zstd -fsys=ncurses

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

test:
	zig build test
	mandoc -T lint ncdu.1
	reuse lint
