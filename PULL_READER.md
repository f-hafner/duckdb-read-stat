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
- reading across the 2048-row chunk boundary with `OFFSET`.

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

- Single-threaded: one reader cursor per query.
- Metadata is parsed in both bind and init; it is never parsed per chunk.
- Arbitrary row starts still scan to the requested offset. A page/row index is
  future work.
- No projection pushdown yet.
- RDC-compressed SAS is not covered by a fixture, though its existing decode
  path was preserved.
- No row buffer; the only buffering is ReadStat's existing page/decompressor
  state.

---

## Adding multithreaded reads

The current design deliberately keeps one reader per query. To add parallel
reads, the reader must be able to start at a known row/page without scanning
from row zero, and each thread must have its own parser and file handle.

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

#### ZSAV: use the zlib trailer as the index

ZSAV already contains a trailer table. Each `ztrailer_entry` maps a compressed
byte range to an uncompressed byte range:

```c
struct ztrailer_entry {
    int64_t uncompressed_ofs;
    int64_t compressed_ofs;
    int32_t uncompressed_size;
    int32_t compressed_size;
};
```

The uncompressed data stream is just consecutive rows. With
`row_len = var_offset * 8`, row `r` starts at uncompressed byte
`r * row_len`.

So the ZSAV index is the trailer itself, sorted by `uncompressed_ofs`:

```c
struct zsav_block_index_entry {
    int64_t first_uncompressed_byte;
    int64_t compressed_offset;
    int32_t uncompressed_size;
    int32_t compressed_size;
};
```

To start at row `r` in a thread:

1. Compute `start_byte = r * row_len`.
2. Binary-search the trailer entry where
   `uncompressed_ofs <= start_byte < uncompressed_ofs + uncompressed_size`.
3. Seek to `compressed_ofs`, read `compressed_size` bytes, and uncompress the
   block.
4. Set the row decoder's `uncompressed_offset` to
   `start_byte - uncompressed_ofs`.
5. Load further blocks as the row/range crosses block boundaries.

No data scan is needed to build this index. A row may cross a block boundary;
the existing ZSAV reader already carries `uncompressed_offset` across blocks, so
that case is handled naturally.

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

Therefore row-compressed SAV should **not** be enabled for parallelism by
default. It only makes sense when profiling shows that typed value decoding,
string conversion, and DuckDB output dominate over RLE expansion. A
producer/consumer pipeline (one decompressor feeding a small queue of decoded
rows) avoids the double expansion but reintroduces buffering and thread
coordination. A persistent per-file index cache is another option if the same
file is read repeatedly.

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

XPORT and POR have no fixed row table or trailer mapping. Supporting them in
parallel means building a row-boundary byte index with a one-time scan; leave
them single-threaded initially.

#### Index build cost

The index is built once per query, not once per chunk, but the cost varies a lot
by format:

| Format | Index build cost | Notes |
|---|---:|---|
| DTA / uncompressed SAV | none | row offset is computed arithmetically |
| ZSAV | O(trailer entries) | trailer is already read during open; cheaper than a data scan |
| SAV RLE | O(N) sequential decode | full decompression pass without value conversion; checkpoint memory is O(N / interval) |
| SAS7BDAT | O(page count) | page headers and subheader pointers only; no row decompression |
| XPORT / POR | O(N) scan | requires a row-boundary byte index |

Because this is a per-query overhead, the index should only be built when the
parallel path is actually selected and the file is large enough to benefit. The
serial path should stay index-free. For very small files, or queries with an
effective `LIMIT`, building an RLE/SAS index can cost more than the serial scan
it replaces.

For prepared statements, the index can optionally be built once in `bind` and
cached in bind data, but that makes bind more expensive and increases bind data
memory. Building it in the global `init` is the simpler default.

### 2. Add a start position to the reader API

Extend the reader API with a start position:

```c
readstat_reader_t *readstat_reader_open_at(
    readstat_parser_t *parser,
    readstat_file_format_t format,
    const char *path,
    int64_t start_row,
    int64_t row_count,
    void *user_ctx,
    readstat_error_t *out_error);
```

`readstat_reader_open()` becomes the special case
`readstat_reader_open_at(..., 0, -1, ...)`. Each format uses the index to seek
directly to the page/block containing `start_row` and skips only within that
page.

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

In local init, each thread creates its own cursor:

```c
void duckdb_read_stat_local_init(duckdb_init_info info) {
    shared_state *shared = duckdb_init_get_bind_data(info);
    stat_range range = claim_next_range(shared);
    duckdb_read_stat_init_data *local = stat_cursor_open_range(shared, range);
    duckdb_init_set_init_data(info, local, duckdb_read_stat_init_data_free);
}
```

In the table function:

```c
duckdb_read_stat_init_data *local =
    duckdb_function_get_local_init_data(info);
```

The C API does not expose an explicit partition object, so a simple approach is
a shared atomic work cursor in the bind/global state:

```c
idx_t task = atomic_fetch_add(&shared->next_task, 1);
```

Each task claims a contiguous row/page range. `duckdb_init_set_max_threads()`
should only be set once cursors can start at arbitrary indexed positions.

### 4. Give each thread its own file handle

The current bind data owns one file handle. That is not safe for concurrent
readers. Store the `duckdb_file_system` in bind data and let each local init open
its own file handle:

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

### Recommended rollout

1. Keep serial readers as the default.
2. Add `readstat_reader_open_at()` and record-offset starts for DTA and
   uncompressed SAV.
3. Add ZSAV starts using its existing zlib trailer entries; no data scan is
   needed.
4. Add row-compressed SAV starts using RLE checkpoints only if profiling shows
   that typed decoding/output dominates RLE expansion. RLE has no seek table, so
   index creation is itself a full expansion pass; default this format to
   single-threaded.
5. Add page-indexed SAS7BDAT starts, including mid-page/subheader cursors.
6. Add XPORT/POR only if a row-boundary index is worth the extra scan.
7. Enable `duckdb_init_set_max_threads()` per format only when that format has
   a working random-access start.
