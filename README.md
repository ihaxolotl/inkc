# Ink C

InkC is a port of Inkle's Ink narrative scripting engine to the C programming language.

The project is written in pure C99 with no non-standard extensions, as God intended.

## Features

InkC is very early in development. As such, a small subset of features are currently supported:
 * Simple content statements
 * Choice statements
 * Gathered choice statements
 * Global variables (`VAR`, `CONST`)
 * Temporary variables (`temp`)
 * Functions / calls
 * Knots / stitches / diverts
 * Arithmetic / logical expressions
 * Inline logic
 * Inline conditionals
 * Switch statements
 * String interpolation
 * Type coersion

A significant deviation from the reference implementation is the inclusion of lexical scoping.

Source files also currently only support the US-ASCII character set.

## Installation

```bash
# Debug
make

# Production
make PROFILE=release
```

InkC does not yet provide a Windows-compatible build, though this need will soon be filled.

By default, the build process uses the LLVM toolchain, with [Clang](https://clang.llvm.org/) as the compiler. Support for other compilers can be achieved by overriding variables within the Makefile.

## Usage

```
inkc [OPTION]... [FILE]
Load and execute an Ink story.

  -h, --help       Print this message
  --colors         Enable color output
  --compile-only   Compile the story without executing
  --dump-ast       Dump a source file's AST
  --dump-story     Dump a story's bytecode
  --trace          Enable execution tracing
  --trace-gc       Enable garbage collector tracing
```

## Contributing

Pull requests are welcome! For major changes, please open an issue first to discuss what you would like to change.

The development toolchain requires the following:
 * LLVM (clang, clang-format, lit)
 * Python
 * Pip
 * CMocka
 * Pre-commit

All commits must first be processed by pre-commit hooks. Before you can run hooks, you must have the pre-commit package manager installed.

Using pip:
```
pip install pre-commit
```

All commit messages should follow the [Conventional Commits](https://www.conventionalcommits.org/en/v1.0.0/) standard.

## License

[MIT](https://choosealicense.com/licenses/mit/)
