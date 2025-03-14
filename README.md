# Ink C
InkC is a port of Inkle's Ink narrative scripting engine to the C programming language.

The project is written in pure C99 with no non-standard extensions, as God intended.

## Installation
```bash
# Debug
make

# Production
make PROFILE=release
```

InkC does not yet provide a Windows-compatible build, though this need will soon be filled.

By default, the build process uses the LLVM toolchain, with [Clang](https://clang.llvm.org/) as the compiler. Support for other compilers can be achieved by overriding variables within the Makefile.

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
TBD
