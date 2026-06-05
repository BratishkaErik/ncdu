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
        .libc_file = if (b.libc_file) |libc_file| .{ .cwd_relative = libc_file } else null,
    });

    const main_mod = b.createModule(.{
        .root_source_file = b.path("src/main.zig"),
        .target = target,
        .optimize = optimize,
        .strip = strip,
        .link_libc = true,
    });

    if (!use_system_ncurses) ncurses: {
        // These are settings used by release process
        // (for tarballs with static binary)
        const ncurses_dep = b.lazyDependency("ncurses", .{
            .target = target,
            .optimize = optimize,

            .linkage = .static,
            .@"no-widechar" = false,
        }) orelse break :ncurses;
        const ncurses_lib = ncurses_dep.artifact("ncurses");
        t.addIncludePath(ncurses_lib.getEmittedIncludeTree());
        main_mod.linkLibrary(ncurses_lib);
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
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

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
