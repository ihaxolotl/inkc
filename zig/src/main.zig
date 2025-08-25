const std = @import("std");
const fs = std.fs;
const fatal = std.process.fatal;
const Allocator = std.mem.Allocator;

var debug_allocator: std.heap.DebugAllocator(.{}) = .init;
const max_src_size = std.math.maxInt(u32);

fn read_source_file(gpa: Allocator, file_reader: *fs.File.Reader) ![:0]u8 {
    var buffer: std.ArrayListUnmanaged(u8) = .empty;
    defer buffer.deinit(gpa);

    if (file_reader.getSize()) |size| {
        const casted_size = std.math.cast(u32, size) orelse return error.StreamTooLong;
        try buffer.ensureTotalCapacityPrecise(gpa, casted_size + 1);
    } else |_| {}

    try file_reader.interface.appendRemaining(gpa, &buffer, .limited(max_src_size));

    const unsupported_boms = [_][]const u8{
        "\xff\xfe\x00\x00", // UTF-32 little endian
        "\xfe\xff\x00\x00", // UTF-32 big endian
        "\xfe\xff", // UTF-16 big endian
    };
    for (unsupported_boms) |bom| {
        if (std.mem.startsWith(u8, buffer.items, bom)) {
            return error.UnsupportedEncoding;
        }
    }
    if (std.mem.startsWith(u8, buffer.items, "\xff\xfe")) {
        if (buffer.items.len % 2 != 0) {
            return error.InvalidEncoding;
        }
        return std.unicode.utf16LeToUtf8AllocZ(gpa, @ptrCast(@alignCast(buffer.items))) catch |err| switch (err) {
            error.DanglingSurrogateHalf => error.UnsupportedEncoding,
            error.ExpectedSecondSurrogateHalf => error.UnsupportedEncoding,
            error.UnexpectedSecondSurrogateHalf => error.UnsupportedEncoding,
            else => |e| return e,
        };
    }
    return buffer.toOwnedSliceSentinel(gpa, 0);
}

fn do_stuff(source: [:0]const u8) void {
    std.debug.print("{s}\n", .{source});
}

pub fn main() !void {
    var stdin_buffer: [4096]u8 align(std.heap.page_size_min) = undefined;

    const gpa = debug_allocator.allocator();
    defer _ = debug_allocator.deinit();

    const args = try std.process.argsAlloc(gpa);
    defer std.process.argsFree(gpa, args);

    if (args.len < 2) {
        fatal("Not enough arguments!", .{});
    }

    var arena_allocator = std.heap.ArenaAllocator.init(gpa);
    defer arena_allocator.deinit();

    const arena = arena_allocator.allocator();
    const filename = args[1];
    const source: [:0]const u8 = s: {
        var file: std.fs.File = std.fs.cwd().openFile(filename, .{}) catch |err| {
            fatal("unable to open file '{s}': {s}", .{ filename, @errorName(err) });
        };
        defer file.close();

        var file_reader: fs.File.Reader = file.reader(&stdin_buffer);
        break :s read_source_file(arena, &file_reader) catch |err| {
            fatal("unable to load file '{s}': {s}", .{ filename, @errorName(err) });
        };
    };

    do_stuff(source);
}
