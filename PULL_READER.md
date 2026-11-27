# Pull-based `read_stat` reader

## Goal

`read_stat` previously re-parsed the entire statistics file for every DuckDB
output chunk. DuckDB asks for 2048 rows at a time, so a full scan was quadratic
in the number of rows. The new design keeps ReadStat's parser state and file
position alive across chunks, so a serial scan is one forward pass.

The implementation is split into two layers:

1. an additive pull-reader API applied to ReadStat through
   `patches/0001-readstat-pull-reader.patch`;
2. a stage-separated DuckDB table function that opens the reader once in
   `init` and pulls one chunk per `function` call.

No row buffer is used. Rows are written directly from ReadStat into DuckDB
vectors.

---

## ReadStat patch architecture

### Public API

The patch adds a new API to `src/readstat.h`:

```c
typedef enum readstat_file_format_e {
    READSTAT_FILE_FORMAT_DTA,
    READSTAT_FILE_FORMAT_SAV,
    READSTAT_FILE_FORMAT_POR,
    READSTAT_FILE_FORMAT_SAS7BDAT,
    READSTAT_FILE_FORMAT_XPORT
} readstat_file_format_t;

typedef struct readstat_reader_s readstat_reader_t;

readstat_reader_t *readstat_reader_open(
    readstat_parser_t *parser,
    readstat_file_format_t format,
    const char *path,
    void *user_ctx,
    readstat_error_t *out_error);

readstat_error_t readstat_reader_read_rows(
    readstat_reader_t *reader,
    size_t max_rows);

readstat_error_t readstat_reader_set_range(
    readstat_reader_t *reader,
    int64_t start_row,
    int64_t row_count);

int readstat_reader_is_eof(const readstat_reader_t *reader);
void readstat_reader_close(readstat_reader_t *reader);
```

`readstat_reader_open()`:

- opens the file through the configured ReadStat IO callbacks,
- parses headers, metadata, variables, and format-specific setup,
- invokes the metadata and variable handlers exactly once,
- positions the parser at the first data row,
- leaves the format context, decompressor/page state, and file position alive.

`readstat_reader_read_rows()` emits at most `max_rows` rows through the value
handler. It returns with the parser positioned exactly where it stopped, so the
next call continues the same scan.

`readstat_reader_set_range()` repositions an open reader at `start_row` for at
most `row_count` rows. It clears the reader's EOF state. DTA and uncompressed
SAV implement it; other formats return
`READSTAT_ERROR_UNSUPPORTED_OPERATION`.

`readstat_reader_close()` performs format-specific finalization, closes the file
through the IO callbacks, and frees the format context and reader.

### Ownership

The reader **borrows** the parser:

- the parser must stay alive until `readstat_reader_close()`,
- `readstat_reader_close()` does not free the parser,
- the caller remains responsible for `readstat_parser_free()`.

This preserves the original `readstat_parse_*` ownership contract.

### Existing parse functions

The existing `readstat_parse_*()` entry points are kept as wrappers:

```c
reader = readstat_reader_open(parser, format, path, ctx, &error);
while (!readstat_reader_is_eof(reader))
    readstat_reader_read_rows(reader, SIZE_MAX);
readstat_reader_close(reader);
```

This keeps the change additive and makes the reader API suitable for upstreaming.

### Persistent state per format

| Format | Persistent state added / moved |
|---|---|
| DTA | `data_started`, `data_finished`, `current_row`, record cursor; `<data>` is consumed on open and `</data>`/value labels on close |
| SAV uncompressed | `data_started`, `data_finished`, reusable row buffer, file position |
| SAV RLE | `sav_rle_state_t`: input buffer, buffer/data offsets, uncompressed row buffer, `sav_row_stream_s` |
| ZSAV | `zsav_state_t`: zlib trailer entries, current block, compressed/uncompressed block buffers, row stream state |
| XPORT | reusable row/blank-row buffers, pending blank-row count, pending row, EOF state |
| POR | token/IO position, `obs_count`, `row_offset`, `row_limit`, EOF state |
| SAS7BDAT | page cursor, page buffer, subheader pointer index, page data area, mid-page row index, row-limit/EOF state |

The SAS reader is the most involved because a page can contain metadata
subheaders, compressed rows, and a trailing row area. The cursor can stop in the
middle of a page and resume from the exact subheader row index on the next call.

### Patch application

The ReadStat submodule stays pinned to upstream. The patch is stored in this
repository and applied before compiling:

```shell
make patch_readstat
make unpatch_readstat
```

`make configure`, `make debug`, and `make release` apply the patch
automatically. The patch target is idempotent and fails loudly if the submodule
has moved and the patch no longer applies.

---

## DuckDB extension architecture

### Stages

| Stage | Work | State |
|---|---|---|
| `bind` | determine format, open a metadata-only reader, build schema and cardinality, close reader | immutable bind data: path, format, encoding, DuckDB file handle |
| `init` | create parser and IO context, open the persistent reader, keep it at the first data row | init data: parser, reader, IO context, output context |
| `function` | pull at most `duckdb_vector_size()` rows and fill the output chunk | reader cursor lives in init data |
| destructor | close reader, free parser, free IO context | — |

### Bind

`duckdb_read_stat_bind()`:

1. reads `path`, `format`, and `encoding`,
2. detects the concrete format and stores both:
   - the extension-level format for date conversions,
   - the ReadStat reader format,
3. opens a DuckDB file handle,
4. runs `readstat_reader_open()` with metadata/variable/error handlers,
5. closes the reader immediately without reading data rows,
6. stores the file handle and format in bind data.

Bind no longer scans data pages just to discover the schema.

### Init

`duckdb_read_stat_init()`:

1. gets the bind data,
2. seeks the file handle to zero,
3. creates a ReadStat parser and an extension IO context,
4. installs IO, metadata, variable, value, and error handlers,
5. sets the file encoding if requested,
6. calls `readstat_reader_open()` and keeps the returned reader,
7. stores the reader, parser, IO context, and output context in init data.

The init data destructor calls `readstat_reader_close()` and then frees the
parser.

### Function

`duckdb_read_stat_function()`:

1. gets the init data,
2. reports any stored reader error,
3. returns an empty chunk if the reader is already at EOF,
4. resets the per-chunk output slot state,
5. calls `readstat_reader_read_rows(reader, duckdb_vector_size())`,
6. sets the output chunk size to the number of rows emitted.

The value handler writes directly into the output vectors. A local slot counter
maps consecutive rows to consecutive DuckDB vector rows; ReadStat's global
`obs_index` is only used to detect row boundaries.

### File and IO ownership

- Bind data owns the DuckDB file handle and destroys it when the bind data is
  destroyed.
- Init data borrows that handle through an extension IO context.
- ReadStat open/close callbacks are no-ops; the extension owns the handle.
- Read and seek callbacks delegate to the DuckDB file API.
- `SEEK_SET`, `SEEK_CUR`, and `SEEK_END` use proper absolute-position
  arithmetic.

### Value conversion

The existing value handler is reused. It supports:

- strings,
- int8/int16/int32,
- float/double,
- SAS/SPSS/Stata date, datetime, and time formats,
- system, tagged, and defined missing values.

The structural changes are that it uses a per-chunk row slot instead of
ReadStat's global observation index, caches the date/time format class and
defined-missing flag per column during bind, and resolves output vector/data
pointers once per chunk instead of once per value.

---

## Testing

Added `test/sql/chunks.test` with 4096-row fixtures (two full DuckDB chunks) for:

- `sas7bdat` uncompressed,
- `sas7bdat` row-compressed,
- `xpt`,
- `sav` uncompressed,
- `sav` row-compressed,
- `zsav`,
- `por`,
- `dta`.

The tests cover:

- full scans across multiple chunks,
- row counts and aggregates,
- reading across the 2048-row chunk boundary with `OFFSET`,
- parallel scans for every fixture format with `PRAGMA threads=4`,
- indexed formats built through `readstat_index_build()`,
- parallel task boundaries and the short final task,
- serial, index-free scans with `PRAGMA threads=1`.

Verified:

```shell
make test_debug
make test_release
```

Both pass. Valgrind on all formats reports no invalid frees and no definite
leaks. A manual full scan of the 906,382-row SAS file from issue #3 completes in
roughly one second.

---

## Current limitations

- An index is built during bind only when DuckDB's `threads` setting is greater
  than 1. With `threads=1`, scans are serial and index-free.
- Metadata is parsed in bind and again once per local reader; it is never parsed
  per chunk or per task.
- Indexed formats pay a one-time index-build pass per query. For row-compressed
  SAV and ZSAV that pass includes decompression.
- No projection pushdown yet.
- RDC-compressed SAS is not covered by a fixture, though its existing decode
  path was preserved.
- No row buffer; the only buffering is ReadStat's existing page/decompressor
  state.

---

## Multithreaded reads

### Implemented

The extension now scans all supported formats in parallel when DuckDB is
configured with more than one thread:

- DTA and uncompressed SAV compute row offsets arithmetically.
- Row-compressed SAV, ZSAV, XPORT, POR, and SAS7BDAT use an immutable index
  built once during bind.

The ReadStat patch adds `readstat_reader_set_range()` so an open reader can be
repositioned at a row range. Each format implements it with either an
arithmetic seek or its index. `readstat_index_build()` constructs an index once
and `readstat_reader_set_index()` attaches it to each local reader.

The implementation works as follows:

- Bind keeps the DuckDB file system instead of a shared file handle. The
  metadata handle used during bind is closed before bind returns. If `threads`
  is greater than 1 and the format needs an index, bind builds the index and
  shares it read-only with every local reader.
- Global init resets a shared atomic task cursor and calls
  `duckdb_init_set_max_threads()` with the planned concurrency.
- Each local init owns an init-data object. The table function opens one
  DuckDB file handle, parser, format context, and pull reader per local
  reader. For each claimed contiguous row range it calls
  `readstat_reader_set_range()` and pulls chunks until the range is
  exhausted. The same reader is then repositioned for the next range; it is
  not reopened.
- Task size is
  `clamp(cardinality / (DUCKDB_READ_STAT_MAX_THREADS * 4), vector_size,
  vector_size * 8)`, where `vector_size` is `duckdb_vector_size()` (normally
  2048). Tasks are therefore one to eight DuckDB chunks. Files below two
  vectors, files with unknown cardinality, and formats without an indexed row
  start use one range and one reader.
- With one thread, no index is built and the serial reader is used.
- Parallel table scans do not preserve file order. Use `ORDER BY` when row
  order matters.

The rest of this document describes the general design and the remaining
per-format work.

To add parallel reads for another format, the reader must be able to start at a
known row/page without scanning from row zero, and each thread must have its own
parser and file handle.

### 1. Build a row/page index once

In bind or init, build an immutable index:

```c
typedef struct stat_index_entry {
    int64_t first_row;
    int64_t file_offset;
    uint64_t row_count;
    uint32_t flags;      /* data / compressed / mix */
} stat_index_entry_t;

typedef struct stat_index {
    stat_index_entry_t *entries;
    size_t count;
} stat_index_t;
```

The index does not have to be one uniform structure. Each format can expose the
smallest structure that lets a cursor resolve `start_row` to a file location.

#### DTA and uncompressed SAV

Rows are fixed-size records. The mapping is either implicit:

```text
file_offset = data_offset + start_row * record_len
```

or stored as a sparse array of row/byte checkpoints. No scan of the data section
is needed.

#### ZSAV: the zlib trailer is not a row index

The ZSAV trailer maps compressed byte ranges to zlib-uncompressed byte ranges.
It does **not** map rows to bytes. ZSAV applies SPSS row compression first and
then zlib-compresses the resulting 8-byte RLE command stream. After inflating a
block, `uncompressed_ofs` points into that RLE command stream, not into
fixed-width rows, so `start_row * row_len` is not a valid trailer lookup.

A ZSAV index can still be checkpointed, but the checkpoint must capture the RLE
decoder state at a row boundary:

```c
struct zsav_checkpoint {
    int64_t first_row;
    int block_index;
    size_t block_data_offset; /* next inflated byte */
    uint8_t chunk[8];         /* pending RLE command block */
    uint8_t chunk_pos;        /* decoder's state.i */
};
```

Building this index requires one inflate + RLE expansion pass without typed
value conversion. Like row-compressed SAV, that pass nearly doubles expansion
work. ZSAV is therefore parallelized when `threads > 1`, but the index-build
cost is part of the query. The trailer alone is not a row index.

#### SAV row compression (RLE): checkpoint the decoder

Row-compressed SAV has no seek table. The data stream is a sequence of 8-byte
RLE commands, and a row boundary can fall in the middle of an 8-byte command
block. The decoder therefore has to be checkpointed during a sequential scan.

A checkpoint needs the row number, the compressed file offset of the next input
byte, and the small amount of decoder state needed to continue:

```c
struct sav_rle_checkpoint {
    int64_t first_row;
    int64_t file_offset;      /* next compressed input byte */
    uint8_t chunk[8];         /* pending 8-byte command block */
    uint8_t chunk_pos;        /* decoder's state.i */
};
```

A simpler variant records checkpoints only where `chunk_pos == 0`, so the
decoder can restart cleanly at a command-block boundary. That uses fewer bytes
per checkpoint but produces fewer restart points.

To build the index, run the RLE decoder once without invoking the value handler,
and record a checkpoint every N rows or every N compressed bytes.

To start at row `r` in a thread:

1. Binary-search the last checkpoint with `first_row <= r`.
2. Seek to `file_offset` and restore `chunk` / `chunk_pos`.
3. Fill the input buffer and decode forward until `r` is reached.
4. Emit rows until the task's end row.

Different threads restart from different checkpoints and never share decoder
state. The checkpoint spacing trades memory and redundant decode work.

The index build itself needs **RLE expansion** (`sav_decompress_row()`), but not
**row decoding** (`sav_process_row()`): it does not need to build typed values,
run iconv, or write DuckDB vectors. That distinction matters a lot for whether
parallel reads are worthwhile.

For a serial scan, the cost is approximately:

```text
serial = RLE expansion + typed row decode + DuckDB value output
```

For an indexed parallel scan, the cost is approximately:

```text
parallel = RLE expansion (index build)
         + RLE expansion (threads, in parallel)
         + typed decode/output (threads, in parallel)
```

So the index does not eliminate RLE expansion; it nearly doubles it. If `f` is
the fraction of serial time spent in RLE expansion, the best possible speedup
with many threads is approximately `1 / f`. If RLE expansion is already half the
time, parallel reads cannot exceed roughly 2x; if it is 80% of the time, they
cannot exceed roughly 1.25x.

A rough prior for a full-column scan is:

| Data shape | Likely RLE-expansion share | Likely dominant cost |
|---|---:|---|
| String-heavy, non-UTF-8 encoding | ~5–20% | string assembly, iconv, output copies |
| Missing/blank/repeated-value heavy | very low | typed decode/output |
| Numeric-only, random/high-entropy doubles | ~30–50% | roughly split between RLE expansion and decode/output |
| Numeric-only with many unprojected columns | can exceed 50% | RLE expansion, because the whole row is still expanded |
| Date/time-formatted numerics | lower | format checks plus date/time conversion |

The usual expectation is that typed row decoding and DuckDB output dominate:
RLE expansion is a tight loop over 8-byte segments, while row decoding adds
segment bookkeeping, string assembly, iconv, missing-value checks, a callback
per variable, type switching, validity handling, and vector writes. The value
handler now caches the date/time format class per column, so it no longer runs
the long chain of format `strcmp`s for every value.

Projection pushdown is the important caveat. RLE expansion always processes the
entire row, even if DuckDB only needs a few columns. If projection pushdown is
added and only 2 of 50 columns are selected, decode/output work drops sharply
while RLE expansion stays constant, so RLE expansion can easily become the
major cost.

Row-compressed SAV is parallelized when `threads > 1`. Because index creation
is itself a full expansion pass, the speedup depends on typed value decoding,
string conversion, and DuckDB output dominating RLE expansion. A
producer/consumer pipeline or persistent per-file index cache could reduce the
double-expansion cost in the future.

#### SAS7BDAT: index pages, then use the page cursor

SAS pages are fixed-size and start at predictable file offsets:

```text
page_offset = header_size + page_index * page_size
```

A useful SAS index is therefore one entry per page:

```c
struct sas_page_index_entry {
    int64_t first_row;
    uint32_t page_index;
    uint32_t row_count;
    uint32_t kind;        /* DATA, MIX, or EMPTY */
};
```

Build it by scanning page headers and subheader pointers once:

- `DATA` page: the page header contains the row count.
- `MIX` page: row count is the number of row-emitting subheaders plus the
  trailing data-area row count.
- metadata/compressed-only pages: no rows.

The scan counts row subheaders; it does not need to decompress them.

To start at row `r` in a thread:

1. Binary-search the page index for `first_row <= r`.
2. Seek to `page_offset` and load the page.
3. Compute `within_page = r - first_row`.
4. For a `DATA` page, set the page row cursor to `within_page`.
5. For a `MIX` page, walk the subheader pointers and skip row-emitting
   subheaders until `within_page` is consumed, then use the trailing data area
   row cursor. Skipped compressed subheaders are skipped by pointer, not
   decompressed.
6. Decode rows until the task's end row or page end.

SAS therefore does not need a byte-level index for compressed row payloads. It
needs a page index plus the existing mid-page cursor, which the pull reader
already implements.

XPORT and POR have no fixed row table or trailer mapping. Their parallel path
builds a sparse row-boundary index with a one-time scan when `threads > 1`.

#### Index build cost

The index is built once per query, not once per chunk, but the cost varies a lot
by format:

| Format | Index build cost | Notes |
|---|---:|---|
| DTA / uncompressed SAV | none | row offset is computed arithmetically |
| ZSAV | O(N) inflate + RLE expansion | trailer locates zlib blocks but not row starts; checkpoints need decoder state |
| SAV RLE | O(N) sequential decode | full decompression pass without value conversion; checkpoint memory is O(N / interval) |
| SAS7BDAT | O(page count) | page headers and subheader pointers only; no row decompression |
| XPORT / POR | O(N) scan | requires a row-boundary byte index |

Because this is a per-query overhead, the current implementation only builds an
index when DuckDB's `threads` setting is greater than 1. The serial path stays
index-free. For very small files, or queries with an effective `LIMIT`, index
creation can cost more than the serial scan it replaces.

For prepared statements, the index can optionally be built once in `bind` and
cached in bind data, but that makes bind more expensive and increases bind data
memory. Building it in the global `init` is the simpler default.

### 2. Reposition an open reader

The reader API now includes:

```c
readstat_error_t readstat_reader_set_range(
    readstat_reader_t *reader,
    int64_t start_row,
    int64_t row_count);
```

It clears the reader's EOF state and repositions it at `start_row` for at most
`row_count` rows (`row_count < 0` means to the end of the file). DTA and
uncompressed SAV implement it directly:

- DTA seeks to `data_rows_offset + start_row * record_len`.
- Uncompressed SAV seeks to
  `data_section_offset + start_row * (var_offset * 8)`.

All other formats return `READSTAT_ERROR_UNSUPPORTED_OPERATION`. A dedicated
`readstat_reader_open_at()` is not required for the current rollout because a
reader can be opened at row zero and immediately repositioned.

### 3. Use DuckDB local init

Register a local init and set the maximum thread count:

```c
duckdb_table_function_set_init(function, duckdb_read_stat_init);
duckdb_table_function_set_local_init(function, duckdb_read_stat_local_init);
duckdb_table_function_set_function(function, duckdb_read_stat_function);
```

In init:

```c
duckdb_init_set_max_threads(info, max_threads);
```

In local init, each thread creates its own empty cursor state:

```c
void duckdb_read_stat_local_init(duckdb_init_info info) {
    duckdb_read_stat_init_data *local = duckdb_read_stat_init_data_create();
    duckdb_init_set_init_data(info, local, duckdb_read_stat_init_data_free);
}
```

The table function opens the parser/reader lazily, then claims ranges and calls
`readstat_reader_set_range()` for each one:

```c
duckdb_read_stat_init_data *local =
    duckdb_function_get_local_init_data(info);
```

The C API does not expose an explicit partition object, so the implementation
uses a shared atomic work cursor in the bind/global state:

```c
idx_t task = atomic_fetch_add(&shared->next_task, 1);
```

Each task claims a contiguous row/page range. `duckdb_init_set_max_threads()`
is set only when the format has a working indexed start.

### 4. Give each thread its own file handle

Bind data keeps the `duckdb_file_system`, not a shared file handle. Each local
reader opens and retains its own file handle:

```c
duckdb_file_system_open(shared->file_system, shared->path, options, &handle);
```

Each local cursor then owns:

- its own `readstat_parser_t`,
- its own format context,
- its own DuckDB file handle,
- its own output slot state.

Bind data and the row/page index stay immutable and shared.

### 5. Ordering

Parallel table scans do not preserve row order. Queries that require file order
must use `ORDER BY`, or the extension must restrict parallelism to 1 unless the
query is order-insensitive.

### Rollout status

1. **Done:** arithmetic row starts for DTA and uncompressed SAV.
2. **Done:** `readstat_reader_set_range()` and persistent per-thread readers.
3. **Done:** per-query task cursor, local init, per-thread parser/file handle,
   and `duckdb_init_set_max_threads()`.
4. **Done:** index creation during bind when `threads > 1` for row-compressed
   SAV, ZSAV, XPORT, POR, and SAS7BDAT.
5. **Done:** RLE and ZSAV checkpoint indexes.
6. **Done:** SAS7BDAT page index with per-page subheader cursors.
7. **Done:** XPORT and POR row-boundary indexes.
8. **Next:** consider projection pushdown and a persistent index cache.
