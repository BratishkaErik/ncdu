// SPDX-FileCopyrightText: Yorhel <projects@yorhel.nl>
// SPDX-License-Identifier: MIT

const std = @import("std");

const Translator = @import("translate_c").Translator;

const manifest = @import("build.zig.zon");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const pie = b.option(bool, "pie", "Build with PIE support (by default: target-dependant)");
    const strip = b.option(bool, "strip", "Strip debugging info (by default false)") orelse false;

    const use_system_ncurses = b.systemIntegrationOption("ncurses", .{});
    const use_system_zstd = b.systemIntegrationOption("zstd", .{});

    var sys_libs: std.ArrayList(Translator.LinkSystemLib) = .empty;
    sys_libs.ensureUnusedCapacity(b.graph.arena, 2) catch @panic("OOM");
    if (use_system_ncurses) {
        sys_libs.appendAssumeCapacity(.{ .name = "ncursesw" });
    }
    if (use_system_zstd) {
        sys_libs.appendAssumeCapacity(.{ .name = "zstd" });
    }

    const translate_c = b.dependency("translate_c", .{});
    const t: Translator = .init(translate_c, .{
        .c_source_file = b.path("src/c.h"),
        .target = target,
        .optimize = optimize,

        .link_system_libs = sys_libs.items,
    });

    const main_mod = b.createModule(.{
        .root_source_file = b.path("src/main.zig"),
        .target = target,
        .optimize = optimize,
        .strip = strip,
        .link_libc = true,
    });

    if (!use_system_ncurses) ncurses: {
        const ncurses_dep = b.lazyDependency("ncurses", .{}) orelse break :ncurses;

        const ncurses = buildNcurses(b, ncurses_dep, target.result, pie);
        t.run.step.dependOn(&ncurses.step.step);
        t.addIncludePath(ncurses.inst_dir.path(b, "include/ncursesw"));
        t.addIncludePath(ncurses.inst_dir.path(b, "include"));
        main_mod.addObjectFile(ncurses.inst_dir.path(b, "lib/libncursesw.a"));
    }

    if (!use_system_zstd) zstd: {
        // These are settings used by release process
        // (for tarballs with static binary)
        const zstd_dep = b.lazyDependency("zstd", .{
            .target = target,
            .optimize = optimize,

            .linkage = .static,
            .strip = strip,
            .pie = pie,

            .compression = true,
            .decompression = true,
            .dictbuilder = false,
            .minify = true,
            .@"exclude-compressors-dfast-and-up" = true,
        }) orelse break :zstd;
        const zstd_lib = zstd_dep.artifact("zstd");
        t.addIncludePath(zstd_lib.getEmittedIncludeTree());
        main_mod.linkLibrary(zstd_lib);
    }

    main_mod.addImport("c", t.mod);

    const build_options = b.addOptions();
    build_options.addOption([:0]const u8, "version", manifest.version);
    main_mod.addOptions("build_options", build_options);

    const exe = b.addExecutable(.{
        .name = "ncdu",
        .root_module = main_mod,
    });
    exe.pie = pie;
    // https://github.com/ziglang/zig/blob/faccd79ca5debbe22fe168193b8de54393257604/build.zig#L745-L748
    if (target.result.os.tag.isDarwin()) {
        // useful for package maintainers
        exe.headerpad_max_install_names = true;
    }
    b.installArtifact(exe);

    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());
    run_cmd.addPassthruArgs();

    const run_step = b.step("run", "Run the app");
    run_step.dependOn(&run_cmd.step);

    const unit_tests = b.addTest(.{
        .root_module = main_mod,
    });
    unit_tests.pie = pie;

    const run_unit_tests = b.addRunArtifact(unit_tests);

    const test_step = b.step("test", "Run unit tests");
    test_step.dependOn(&run_unit_tests.step);
}

fn gnuFormat(gpa: std.mem.Allocator, target: std.Target) []const u8 {
    var aw: std.Io.Writer.Allocating = .init(gpa);
    defer aw.deinit();

    const arch_str = switch (target.cpu.arch) {
        .bpfel => "bpf",
        .thumb => "arm",
        .thumbeb => "armeb",
        // Same or untested
        else => |same| @tagName(same),
    };

    const os_str = switch (target.os.tag) {
        else => |same| @tagName(same),
    };

    const abi_str = switch (target.abi) {
        else => |same| if (target.os.tag == .macos)
            "darwin"
        else
            @tagName(same),
    };

    return std.fmt.allocPrint(gpa, "{s}-{s}-{s}", .{ arch_str, os_str, abi_str }) catch @panic("OOM");
}

fn zigFormat(gpa: std.mem.Allocator, target: std.Target) []const u8 {
    return target.zigTriple(gpa) catch @panic("OOM");
}

const NcursesResult = struct {
    step: *std.Build.Step.Run,
    inst_dir: std.Build.LazyPath,
};

// https://ziggit.dev/t/running-a-configure-script-as-a-build-step-trying-to-build-hdf5/5540/2
// https://ziggit.dev/t/migrating-from-autotools-to-zig-build/7580/3
// TODO: try to find more feature-ful packaged Zig build system support for ncurses.
// Something like https://github.com/allyourcodebase/zstd in our root project dependencies...

/// This is package for dealing with fetched Ncurses library, in case ncdu is being built
/// without system library integration enabled, or when making static tarball release.
fn buildNcurses(
    b: *std.Build,
    ncurses_dep: *std.Build.Dependency,
    target: std.Target,
    pie: ?bool,
) NcursesResult {
    const target_gnu_format = gnuFormat(b.graph.arena, target);
    const target_zig_format = zigFormat(b.graph.arena, target);

    const host_gnu_format = gnuFormat(b.graph.arena, b.graph.host.result);
    const host_zig_format = zigFormat(b.graph.arena, b.graph.host.result);

    const ncurses_cache = b.addNamedWriteFiles(b.fmt("ncurses-{s}", .{target_zig_format}));
    const cwd = ncurses_cache.getDirectory();

    const run_config = std.Build.Step.Run.create(b, "configure fetched ncurses");
    run_config.setCwd(cwd);
    run_config.addFileArg(ncurses_dep.path("configure"));
    const prefix_dir = run_config.addOutputDirectoryArg2("inst", .{ .prefix = "--prefix=", .make_absolute = true });

    run_config.addArgs(&.{
        // We need minimized and static build, so disable unneccessary stuff:

        "--without-cxx",
        "--without-cxx-binding",
        "--without-ada",
        "--without-manpages",
        "--without-progs",
        "--without-tests",
        "--disable-pc-files",
        "--disable-db-install",
        "--without-pkg-config",
        "--without-shared",
        "--without-debug",
        "--without-gpm",
        "--without-sysmouse",
        "--enable-widec",
        "--with-default-terminfo-dir=/usr/share/terminfo",
        "--with-terminfo-dirs=/usr/share/terminfo:/lib/terminfo:/usr/local/share/terminfo",
        "--with-fallbacks=screen linux vt100 xterm xterm-256color",
    });

    // Set info for cross-compilation itself
    run_config.addArgs(&.{
        b.fmt("--host={s}", .{host_gnu_format}),
        b.fmt("--build={s}", .{target_gnu_format}),
    });

    const zig_cc = b.fmt("{s} cc", .{b.graph.zig_exe});
    const zig_ar = b.fmt("{s} ar", .{b.graph.zig_exe});
    const zig_ranlib = b.fmt("{s} ranlib", .{b.graph.zig_exe});

    run_config.setEnvironmentVariable("BUILD_CC", b.fmt("{s} --target={s}", .{ zig_cc, host_zig_format }));
    run_config.setEnvironmentVariable("BUILD_LD", b.fmt("{s} --target={s}", .{ zig_cc, host_zig_format }));

    run_config.setEnvironmentVariable("CC", b.fmt("{s} --target={s}", .{ zig_cc, target_zig_format }));
    run_config.setEnvironmentVariable("LD", b.fmt("{s} --target={s}", .{ zig_cc, target_zig_format }));
    run_config.setEnvironmentVariable("AR", zig_ar);
    run_config.setEnvironmentVariable("RANLIB", zig_ranlib);

    const cflags = std.mem.join(b.graph.arena, " ", &.{
        b.fmt("--target={s}", .{target_zig_format}),
        "-O2",
        if (pie orelse false)
            "-fPIC"
        else
            "",
    }) catch @panic("OOM");
    const ldflags = b.fmt("--target={s}", .{target_zig_format});

    run_config.setEnvironmentVariable("CPPFLAGS", "-D_GNU_SOURCE");
    run_config.setEnvironmentVariable("CFLAGS", cflags);
    run_config.setEnvironmentVariable("LDFLAGS", ldflags);

    // Debugging:
    // run_config.stdio = .inherit;
    // run_config.setStdIn(.none);

    const run_make = std.Build.Step.Run.create(b, "build fetched ncurses");
    run_make.setCwd(cwd);
    run_make.step.dependOn(&run_config.step);

    const make_jobs = 8;
    run_make.addArgs(&.{ "make", b.fmt("-j{d}", .{make_jobs}), "install" });

    // Debugging:
    // run_make.stdio = .inherit;
    // run_make.setStdIn(.none);

    return .{
        .step = run_make,
        .inst_dir = prefix_dir,
    };
}
