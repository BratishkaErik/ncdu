# SPDX-FileCopyrightText: Yorhel <projects@yorhel.nl>
# SPDX-License-Identifier: MIT

# Optional semi-standard Makefile with some handy tools.
# Ncdu itself can be built with just the zig build system.

ZIG ?= zig

PREFIX ?= /usr/local
BINDIR ?= ${PREFIX}/bin
MANDIR ?= ${PREFIX}/share/man/man1
ZIG_FLAGS ?= --release=fast -Dstrip -fsys=zstd -fsys=ncurses

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


static-%.tar.gz:
	@echo "=> Building static binary for $*..."
	$(ZIG) build -fno-sys=zstd -fno-sys=ncurses \
	-Dtarget=$* -Dcpu=baseline --release=fast -Dstrip -Dpie \
	--build-id=fast --prefix static-$*
	@# My system's strip can't deal with arm binaries and zig doesn't wrap a strip alternative.
	@# Whatever, just let it error for those.
	strip -R .eh_frame -R .eh_frame_hdr static-$*/bin/ncdu || true
	@echo "=> Packaging $@..."
	cd static-$* && mv bin/ncdu ncdu && tar -czf ../static-$*.tar.gz ncdu
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
