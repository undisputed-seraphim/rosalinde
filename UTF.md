# @UTF Binary Table Format

The CRIWARE `@UTF` table is a column-major binary serialization format used throughout
Vanillaware's Unicorn Overlord CPK archive. Every CPK TOC, ACB container, and nested
subtable uses this format.

## Binary layout

```
┌────────────────────╥────────────────────────────────────────────┐
│  chunk_header       │                                          │
│  (40 bytes)        ║  column_defs  │  row_data  │  pools      │
│                     │                                              │
│  magic     4B      │                                              │
│  table_size 4B     │                                              │
│  rows_offset 4B ───┼──────────────┐                               │
│  string_offset 4B ─┼──────────────┼───────────────────┐           │
│  data_offset 4B ───┼──────────────┼───────────────────┼───────┐   │
│  table_name 4B     │                                              │
│  num_columns 2B    │                                              │
│  row_length 2B     │                                              │
│  num_rows 4B       │                                              │
├────────────────────╥──┐                                           │
│ per column:           │                                           │
│   flags        1B    │                                           │
│   [name_offset 4B]   │  (if flags & 0x10)                        │
│   [default_value]    │  (if flags & 0x20, one typed value)       │
├──────────────────────╥──┤                                        │
│ per row:                │                                        │
│   for each valid col:  │                                        │
│     typed_value        │                                        │
├────────────────────────╥──┤                                     │
│ string pool:              │                                     │
│   null-terminated strings│                                     │
├──────────────────────────╥──┤                                   │
│ data pool:                  │                                   │
│   binary blobs              │                                   │
└─────────────────────────────┴──────────────────────────────────┘
```

### chunk_header (40 bytes, packed)

| Offset | Type     | Name           | Description                                   |
|--------|----------|----------------|-----------------------------------------------|
| 0x00   | char[4]  | magic          | `"@UTF"`                                      |
| 0x04   | uint32   | table_size     | Total table size in bytes (including header)   |
| 0x08   | uint32   | rows_offset    | Offset to row data, relative to byte 8         |
| 0x0C   | uint32   | string_offset  | Offset to string pool, relative to byte 8      |
| 0x10   | uint32   | data_offset    | Offset to data pool, relative to byte 8        |
| 0x14   | uint32   | table_name     | String pool index for table name               |
| 0x18   | uint16   | num_columns    | Number of column definitions                   |
| 0x1A   | uint16   | row_length     | Byte size of one row in the row data block     |
| 0x1C   | uint32   | num_rows       | Number of data rows                            |

All multi-byte values are **big-endian**. The three `*_offset` fields are relative to
**byte 8 of the table** (immediately after magic and table_size). To obtain
file-absolute offsets, add 8.

### Column definitions

Immediately after the 40-byte chunk header, there are `num_columns` column definitions.
Each consists of:

| Field         | Size   | Condition           | Description                       |
|---------------|--------|---------------------|-----------------------------------|
| flags         | 1B     | always              | See flag table below              |
| name_offset   | 4B     | `flags & 0x10`      | String pool index for column name |
| default_value | varies | `flags & 0x20`      | One typed value (see type table)  |

#### Flags byte

```
bit 7 ─── (unused)
bit 6 ─── is_valid   (0x40) — column contains data; rows will read values for it
bit 5 ─── has_default (0x20) — column has a single default value stored here
bit 4 ─── has_name    (0x10) — column has a name in the string pool
bits 3-0 ─ type       (0x0F) — field type (see below)
```

Key invariant: if `has_default` is set, the column stores **one** value inline
in the column definition. This value is NOT repeated in the row data block.
Columns without `has_default` AND with `is_valid` will read one value per
row from the row data block.

Columns with `is_valid == false` have no data at all — they exist in the schema
but have zero values.

### Type table

| Value | Type     | C++ type                   | Byte size | Notes                |
|-------|----------|----------------------------|-----------|----------------------|
| 0x0   | UINT8    | uint8_t                    | 1         |                      |
| 0x1   | INT8     | int8_t                     | 1         |                      |
| 0x2   | UINT16   | uint16_t                   | 2         | big-endian           |
| 0x3   | INT16    | int16_t                    | 2         | big-endian           |
| 0x4   | UINT32   | uint32_t                   | 4         | big-endian           |
| 0x5   | INT32    | int32_t                    | 4         | big-endian           |
| 0x6   | UINT64   | uint64_t                   | 8         | big-endian           |
| 0x7   | INT64    | int64_t                    | 8         | big-endian           |
| 0x8   | FLOAT    | float                      | 4         | big-endian           |
| 0x9   | DOUBLE   | double                     | 8         | big-endian           |
| 0xA   | STRING   | uint32 offset             | 4         | index into string pool |
| 0xB   | DATA     | uint32 offset, uint32 size | 8         | index into data pool   |

For **STRING**: the stored value is a 4-byte offset into the string pool. The parser
reads the offset, adds `string_offset`, seeks to that position, and reads a
null-terminated string.

For **DATA**: the stored value is an 8-byte pair (offset, size). Both are added to
`data_offset` to locate the binary blob. This is how nested @UTF tables are stored —
the column value is a `data_t { offset, size }` pointing to another @UTF blob.

### Row data block

Located at `rows_offset + 8` from the start of the table.

For `i = 0` to `num_rows - 1`:
- For each column `j = 0` to `num_columns - 1`:
  - If column `j` has `is_valid && !has_default`:
    - Read one value of column `j`'s type

The order is **row-major within the block**: all values for row 0, then all
values for row 1, etc. The `row_length` field gives the total byte stride per row
and can be used to skip entire rows if needed.

Columns have variable-length values (strings are 4B offsets, data is 8B), so
`row_length` is critical for knowing how many bytes to read per row.

### String pool

Located at `string_offset + 8` from the start of the table.

A contiguous block of null-terminated strings. Offsets in STRING values and
column name_offset fields are indices into this block.

### Data pool

Located at `data_offset + 8` from the start of the table.

A contiguous block of binary data. Offsets in DATA values are indices into
this block. Nested @UTF tables serialise their binary output and place it here.

## Encryption

If the first 4 bytes after decipher are `{0x1F, 0x9E, 0xF3, 0xF5}` instead of
`"@UTF"`, the table is XOR-encrypted with a linear congruential stream cipher:

```
j0 = 0x655F
for each byte i: bytes[i] ^= (j & 0xFF); j *= 0x4115
```

The CPK container applies this cipher to the TOC and content tables. The
`UTF::decipher()` function detects the magic bytes and decrypts in place.

## Column-major nature

The @UTF format stores data in a **column-major** arrangement: each column is
a list of values. However, the **binary layout** is actually row-major within
the row data block (values for row 0 come first, then row 1, etc.). The parser
reassembles this into column-major form:

```
Binary layout (row-major):       In-memory layout (column-major):
  Row0: ColA ColB ColC             ColA: [A0, A1, A2]
  Row1: ColA ColB ColC             ColB: [B0, B1, B2]
  Row2: ColA ColB ColC             ColC: [C0, C1, C2]
```

This is a key insight: only the row data block is row-major. The in-memory
representation (`UTF` / `table<Schema>`) is column-major. Nested subtables
are stored as DATA blobs — a single row with a binary blob that can be
re-parsed as another @UTF.

## Current implementation

The parser lives in `libcriware/src/utf.cpp` as `UTF::operator>>(std::istream&)`.
It uses a `std::istream` interface with seek/tell for string and data pool access.
The `UTF` class stores the parsed result as `std::map<std::string, field>` where
each `field` is a `std::vector<std::variant<10 types>>`.

The higher-level `table<Schema>` (in utf.hpp) wraps this: it parses a raw `UTF`,
then `populate()` copies column values into a typed `std::tuple<std::vector<T>...>`.

### Known issues

1. Iostream-based: uses seeking, not span-based. The project prefers `byte_reader.hpp`.
2. No validation beyond magic bytes — corrupted tables produce garbage silently.
3. Field state is mutable during parsing (type_, has_default, valid are set incrementally).
4. Offset arithmetic (+8) is manual and error-prone.
5. `data_as_subtable()` does a stream seek — fragile and coupled to the underlying stream.
6. The `field` class mixes parsing state and storage state.
