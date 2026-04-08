const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    // Shared library
    const lib_mod = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    lib_mod.addCSourceFiles(.{
        .files = &.{
            "c/parser.c",
            "c/lexer.c",
            "c/arena.c",
        },
        .flags = &.{
            "-std=c23",
            "-Wall",
            "-Wextra",
            "-Wpedantic",
            "-fvisibility=hidden",
        },
    });
    lib_mod.addIncludePath(b.path("c"));

    const lib = b.addLibrary(.{
        .name = "sqlparser",
        .linkage = .dynamic,
        .root_module = lib_mod,
    });
    b.installArtifact(lib);

    // Zig tests
    const tests = b.addTest(.{
        .root_module = b.createModule(.{
            .root_source_file = b.path("c/tests.zig"),
            .target = target,
            .optimize = optimize,
            .link_libc = true,
        }),
    });
    tests.root_module.addIncludePath(b.path("c"));
    tests.root_module.linkLibrary(lib);

    const run_tests = b.addRunArtifact(tests);
    const test_step = b.step("test", "Run SQL parser tests");
    test_step.dependOn(&run_tests.step);
}
