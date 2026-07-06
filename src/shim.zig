// SPDX-FileCopyrightText: Zig contributors
// SPDX-License-Identifier: MIT

// Copied and adapted from Zig 0.15.2 API:
// 1. std.fs.File
// 2. std.posix
// 3. std.c

const std = @import("std");
const builtin = @import("builtin");
const native_os = builtin.target.os.tag;
const errno = std.posix.errno;
const system = std.posix.system;
const SEEK = std.posix.SEEK;
const unexpectedErrno = std.posix.unexpectedErrno;
const windows = std.os.windows;
const wasi = std.os.wasi;
const lfs64_abi = native_os == .linux and builtin.link_libc and (builtin.abi.isGnu() or builtin.abi.isAndroid());
const fd_t = std.posix.fd_t;

const File = std.Io.File;

/// Repositions read/write file offset relative to the end.
pub fn seekFromEnd(self: File, offset: i64) !void {
    return lseek_END(self.handle, offset);
}

pub fn getPos(self: File) !u64 {
    return lseek_CUR_get(self.handle);
}

/// Repositions read/write file offset relative to the end.
fn lseek_END(fd: fd_t, offset: i64) !void {
    if (native_os == .linux and !builtin.link_libc and @sizeOf(usize) == 4) {
        var result: u64 = undefined;
        switch (errno(system.llseek(fd, @bitCast(offset), &result, SEEK.END))) {
            .SUCCESS => return,
            .BADF => unreachable, // always a race condition
            .INVAL => return error.Unseekable,
            .OVERFLOW => return error.Unseekable,
            .SPIPE => return error.Unseekable,
            .NXIO => return error.Unseekable,
            else => |err| return unexpectedErrno(err),
        }
    }
    if (native_os == .windows) {
        return windows.SetFilePointerEx_END(fd, offset);
    }
    if (native_os == .wasi and !builtin.link_libc) {
        var new_offset: wasi.filesize_t = undefined;
        switch (wasi.fd_seek(fd, offset, .END, &new_offset)) {
            .SUCCESS => return,
            .BADF => unreachable, // always a race condition
            .INVAL => return error.Unseekable,
            .OVERFLOW => return error.Unseekable,
            .SPIPE => return error.Unseekable,
            .NXIO => return error.Unseekable,
            .NOTCAPABLE => return error.AccessDenied,
            else => |err| return unexpectedErrno(err),
        }
    }
    const lseek_sym = if (lfs64_abi) system.lseek64 else system.lseek;
    switch (errno(lseek_sym(fd, @bitCast(offset), SEEK.END))) {
        .SUCCESS => return,
        .BADF => unreachable, // always a race condition
        .INVAL => return error.Unseekable,
        .OVERFLOW => return error.Unseekable,
        .SPIPE => return error.Unseekable,
        .NXIO => return error.Unseekable,
        else => |err| return unexpectedErrno(err),
    }
}

/// Returns the read/write file offset relative to the beginning.
fn lseek_CUR_get(fd: fd_t) !u64 {
    if (native_os == .linux and !builtin.link_libc and @sizeOf(usize) == 4) {
        var result: u64 = undefined;
        switch (errno(system.llseek(fd, 0, &result, SEEK.CUR))) {
            .SUCCESS => return result,
            .BADF => unreachable, // always a race condition
            .INVAL => return error.Unseekable,
            .OVERFLOW => return error.Unseekable,
            .SPIPE => return error.Unseekable,
            .NXIO => return error.Unseekable,
            else => |err| return unexpectedErrno(err),
        }
    }
    if (native_os == .windows) {
        return windows.SetFilePointerEx_CURRENT_get(fd);
    }
    if (native_os == .wasi and !builtin.link_libc) {
        var new_offset: wasi.filesize_t = undefined;
        switch (wasi.fd_seek(fd, 0, .CUR, &new_offset)) {
            .SUCCESS => return new_offset,
            .BADF => unreachable, // always a race condition
            .INVAL => return error.Unseekable,
            .OVERFLOW => return error.Unseekable,
            .SPIPE => return error.Unseekable,
            .NXIO => return error.Unseekable,
            .NOTCAPABLE => return error.AccessDenied,
            else => |err| return unexpectedErrno(err),
        }
    }
    const lseek_sym = if (lfs64_abi) system.lseek64 else system.lseek;
    const rc = lseek_sym(fd, 0, SEEK.CUR);
    switch (errno(rc)) {
        .SUCCESS => return @bitCast(rc),
        .BADF => unreachable, // always a race condition
        .INVAL => return error.Unseekable,
        .OVERFLOW => return error.Unseekable,
        .SPIPE => return error.Unseekable,
        .NXIO => return error.Unseekable,
        else => |err| return unexpectedErrno(err),
    }
}

const mem = std.mem;
const E = std.posix.E;
const max_path_bytes = std.Io.Dir.max_path_bytes;

/// Same as `realpath` except `pathname` is WTF16LE-encoded.
///
/// The result is encoded as [WTF-8](https://simonsapin.github.io/wtf-8/).
///
/// Calling this function is usually a bug.
pub fn realpathW(pathname: []const u16, out_buffer: *[max_path_bytes]u8) ![]u8 {
    const w = windows;

    const dir = std.Io.Dir.cwd().fd;
    const access_mask = w.GENERIC_READ | w.SYNCHRONIZE;
    const share_access = w.FILE_SHARE_READ | w.FILE_SHARE_WRITE | w.FILE_SHARE_DELETE;
    const creation = w.FILE_OPEN;
    const h_file = blk: {
        const res = w.OpenFile(pathname, .{
            .dir = dir,
            .access_mask = access_mask,
            .share_access = share_access,
            .creation = creation,
            .filter = .any,
        }) catch |err| switch (err) {
            error.WouldBlock => unreachable,
            else => |e| return e,
        };
        break :blk res;
    };
    defer w.CloseHandle(h_file);

    return std.os.getFdPath(h_file, out_buffer);
}

/// Same as `realpath` except `pathname` is null-terminated.
///
/// Calling this function is usually a bug.
pub fn realpathZ(pathname: [*:0]const u8, out_buffer: *[max_path_bytes]u8) ![]u8 {
    if (native_os == .windows) {
        const pathname_w = try windows.cStrToPrefixedFileW(null, pathname);
        return realpathW(pathname_w.span(), out_buffer);
    }
    const result_path = std.c.realpath(pathname, out_buffer) orelse switch (@as(E, @enumFromInt(std.c._errno().*))) {
        .SUCCESS => unreachable,
        .INVAL => unreachable,
        .BADF => unreachable,
        .FAULT => unreachable,
        .ACCES => return error.AccessDenied,
        .NOENT => return error.FileNotFound,
        .OPNOTSUPP => return error.NotSupported,
        .NOTDIR => return error.NotDir,
        .NAMETOOLONG => return error.NameTooLong,
        .LOOP => return error.SymLinkLoop,
        .IO => return error.InputOutput,
        else => |err| return unexpectedErrno(err),
    };
    return mem.sliceTo(result_path, 0);
}
