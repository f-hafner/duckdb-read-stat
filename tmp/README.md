# readstat scratch program

`main.c` is a small test program that parses a Stata `.dta` file with
[readstat](../third_party/readstat) and prints the first 10 rows as
tab-separated values, skipping the `sector` column. It links against
`libread_stat.so`, which is built by the parent DuckDB extension.

## Prerequisites

Build the extension's release target first so that
`../build/release/libread_stat.so` exists:

```bash
# from the repository root
make release
```

## Build

```bash
cd tmp
cmake -B build
cmake --build build
```

This produces `tmp/build/test_bin`. The build RPATH points at
`../build/release`, so no `LD_LIBRARY_PATH` is needed.

## Run

```bash
./build/test_bin test.dta
# or any other .dta file:
./build/test_bin /path/to/file.dta
```
