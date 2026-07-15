---
title: "AutoSql Format Specification"
---

AutoSql is a small specification language used throughout the UCSC Genome Browser to describe
the columns of a table or the fields of an annotation file. A single AutoSql file (conventionally
given the `.as` extension) defines the name, type, and a human-readable description of each
field. In particular, they are used to add extra fields to a
[bigBed](/goldenPath/help/bigBed.html) or [bigGenePred](/goldenPath/help/bigGenePred.html) track:
`bedToBigBed` reads the `.as` file (via its `-as=` option) to learn the names and types of the
fields beyond the standard BED columns, and the Genome Browser uses those descriptions to label
values on item detail pages and in filter menus. This page is aimed at track creators and
hub developers make use of autoSql's more advanced features.


## A simple example

Let's start with a basic [BED format](/FAQ/FAQformat.html#format1) example.
A 6-column BED record stores a position (chromosome, start, end), a name, a score,
and a strand. Its AutoSql specification looks like this:

```
table bed6
"Browser Extensible Data, 6-column variant"
    (
    string chrom;      "Reference sequence chromosome or scaffold"
    uint chromStart;   "Start position in chromosome"
    uint chromEnd;     "End position in chromosome"
    string name;       "Name of item"
    uint score;        "Score from 0-1000"
    char[1] strand;    "+ or - for strand"
    )
```

The format is a hybrid between a C structure and a SQL table definition. Each declaration begins
with a type (`table`, `object`, or `simple`), an object name, and a quoted description, followed
by a parenthesized list of fields. Every field has a type, a name, and its own quoted comment.

From this specification, `autoSql` produces the SQL table definition:

```sql
#Browser Extensible Data, 6-column variant
CREATE TABLE bed6 (
    chrom varchar(255) not null,        # Reference sequence chromosome or scaffold
    chromStart int unsigned not null,   # Start position in chromosome
    chromEnd int unsigned not null,     # End position in chromosome
    name varchar(255) not null,         # Name of item
    score int unsigned not null,        # Score from 0-1000
    strand char(1) not null,            # + or - for strand
              #Indices
    PRIMARY KEY(chrom)
);
```

and the matching C structure:

```c
struct bed6
/* Browser Extensible Data, 6-column variant */
    {
    struct bed6 *next;         /* Next in singly linked list. */
    char *chrom;               /* Reference sequence chromosome or scaffold */
    unsigned chromStart;       /* Start position in chromosome */
    unsigned chromEnd;         /* End position in chromosome */
    char *name;                /* Name of item */
    unsigned score;            /* Score from 0-1000 */
    char strand[2];            /* + or - for strand */
    };
```

Along with the structure, `autoSql` generates functions to load a row from the database, save a
structure in tab- or comma-separated form, and free dynamically allocated structures (for
example, `bed6Load`, `bed6TabOut`, and `bed6Free`). `bed6Load` reads one row from the database
into a `bed6` structure, `bed6TabOut` writes a structure back out as a tab-separated line, and
`bed6Free` releases the memory it used. This saves writing the many lines of repetitive
field-by-field conversion code that reading and writing such records would otherwise require.

## Anatomy of a declaration

Every AutoSql object follows the same shape:

```
<declareType> <name> [index] [auto]
"<description>"
    (
    <fieldType> <fieldName>;   "<field description>"
    ...
    )
```

- **declareType**: one of `table`, `object`, or `simple`. See [Object types](#object-types).
- **name**: a valid identifier: letters, digits, and underscores, starting with a letter.
- **description**: a double-quoted string describing the object, used as a comment in the
  generated SQL and C.
- **fields**: one per line, each with a type, a name, a semicolon, and a double-quoted comment.

The quoted comments are not optional decoration. They are required, and the Genome Browser
surfaces the per-field comments to users, so write them as if an end user will read them.

## Field types

AutoSql supports the following basic field types:

| Type | Description |
| - | - |
| `int` | 32-bit signed integer |
| `uint` | 32-bit unsigned integer |
| `short` | 16-bit signed integer |
| `ushort` | 16-bit unsigned integer |
| `byte` | 8-bit signed integer |
| `ubyte` | 8-bit unsigned integer |
| `bigint` | 64-bit integer |
| `float` | single-precision IEEE floating point |
| `double` | double-precision IEEE floating point |
| `char` | 8-bit character (can only be used in an array) |
| `string` | variable-length string, up to 255 bytes |
| `lstring` | "long string", variable-length string up to 2 billion bytes |
| `enum` | enumerated type holding a single symbolic value |
| `set` | set type holding multiple symbolic values |

In addition, the `simple`, `object`, and `table` types may be used as fields, allowing one object
to contain another. See a more complex [example](#a-more-complicated-example) below.

## Arrays

An array is declared by placing a size in square brackets between the field type and the field
name. The size may be either a fixed number or the name of a previously declared field, which
gives a variable-length array:

```
char[2] state;             "Fixed-length array of 2 characters"
int pointCount;            "Number of points"
uint[pointCount] points;   "Variable-length array sized by pointCount"
```

When the size is a field name, that field must be declared before the array that uses it.

A `char` array is the standard way to store a short fixed-width string, such as the strand
character in a BED-like file (`char[1] strand;`).

## Enum and set fields

The `enum` and `set` types map to SQL enum and set columns and provide compact symbolic values.
Each symbolic value must be a valid C identifier. For an `enum`, a field holds exactly one of the
listed values; for a `set`, a field may hold any combination, represented as a comma-separated
string when loaded from text.

```
table symbolCols
"example of enum and set symbolic columns"
    (
    int id;                                          "unique id"
    enum(male, female) sex;                          "enumerated column"
    set(cProg,javaProg,pythonProg,awkProg) skills;   "set column"
    )
```

In this example, `sex` is an `enum`, so each row is either `male` or `female` and never both.
`skills` is a `set`, so one row can combine several of the listed values, such as
`cProg,pythonProg`.

This is a common pattern for real Genome Browser data. For example, the bigDbSnp format uses an
enum to record each variant's class:

```
enum(snv, mnv, ins, del, delins, identity) class;  "Variation class/type"
```

`autoSql` generates a C enum for the symbolic values and stores a `set` as an unsigned bit field.

## Indexes and autoincrement

If you do not specify any index, `autoSql` assumes the first field is the primary key and indexes
it. To control indexing, add one of the following keywords after a field name:

- `primary`: make the field the primary key.
- `unique`: index a field whose values are all unique.
- `index`: index a field that may contain duplicate values.

You can index just the first part of a long field by following the keyword with a character count
in brackets, for example `index[12]`. To make an integer field auto-incrementing, add the `auto`
keyword after the index keyword, if there is one.

```
table bedIndexed
"BED-like record with explicit indexes"
    (
    uint id primary auto;     "Autoincrementing primary key for this record"
    string name unique;       "Name of item - must be unique"
    string chrom index;       "Reference sequence chromosome or scaffold"
    uint chromStart index;    "Start position in chromosome"
    uint chromEnd;            "End position in chromosome"
    lstring description index[12];  "Item description - indexing just the first 12 characters"
    )
```

In `bedIndexed` above, `id` is declared `auto`, so MySQL fills in its value on its own, counting
up by one for each new row. You never supply an `id` yourself.

## Object types

AutoSql has three kinds of objects, which differ in what code is generated and in how arrays of
them are stored in memory:

| Type | Generates SQL? | Array storage in memory |
| - | - | - |
| `simple` | No | C array |
| `object` | No | Singly linked list |
| `table` | Yes (SQL + C) | Singly linked list |

- **`simple`**: objects that do not appear in lists. An array of a `simple` object is stored in
  memory as a C array. (Historically `simple` objects could not contain strings or
  variable-sized arrays; that restriction has been lifted.)
- **`object`**: like `simple`, but a `next` pointer is automatically inserted as the first field
  of the generated C structure, and what looks like an array in the `.as` file becomes a singly
  linked list in memory.
- **`table`**: like `object`, but `autoSql` also generates a SQL table definition. This is the
  type used for anything stored in the database.

A C array stores its elements one after another in a single block of memory, so you can reach any
element directly by its position. A singly linked list stores each element separately and chains
them together, so reaching the third element means following the chain from the start.

For example, given `point[3]` as a field, declaring `point` as `simple` stores the three points
as a C array, while declaring it as `object` stores them as a linked list.

## A more complicated example

Objects can contain other objects, which makes it possible to describe nested structures such as
a gene locus built from transcripts, each of which is in turn built from exons:

```
simple exon
"A single exon"
    (
    int start;   "Start position of exon in chromosome"
    int end;     "End position of exon in chromosome"
    )

object transcript
"A transcript made up of one or more exons"
    (
    string name;                   "Transcript accession"
    char[1] strand;                "+ or - for strand"
    int exonCount;                 "Number of exons in transcript"
    simple exon[exonCount] exons;  "Array of exons"
    )

table gene
"A gene locus with one or more transcripts"
    (
    string chrom;                                    "Reference sequence chromosome or scaffold"
    int chromStart;                                  "Start position of locus in chromosome"
    int chromEnd;                                    "End position of locus in chromosome"
    string name;                                     "Gene symbol or identifier"
    int transcriptCount;                             "Number of transcripts"
    object transcript[transcriptCount] transcripts;  "List of transcripts"
    )
```

Here `gene` is a `table`, so it gets both SQL and C output, while `transcript` and `exon` are
component objects referenced by its fields. Because `exon` is declared `simple`, the `exons`
array is stored in memory as a C array within each transcript; because `transcript` is declared
`object`, the `transcripts` of a gene form a singly linked list.

For a real Genome Browser format that uses this kind of nesting, see
[`txGraph.as`](https://github.com/ucscGenomeBrowser/kent/blob/master/src/hg/lib/txGraph.as), which
nests `simple` and `object` types to describe a transcription graph.

## See also

- [bigBed format](/goldenPath/help/bigBed.html): uses an `.as` file to define extra fields.
- [bigGenePred format](/goldenPath/help/bigGenePred.html): a worked example of a custom `.as`
  definition passed to `bedToBigBed`.
- [Data file formats FAQ](/FAQ/FAQformat.html): the standard Genome Browser file formats.
- [`autoSql.doc`](https://github.com/ucscGenomeBrowser/kent/blob/master/src/hg/autoSql/autoSql.doc):
  the original AutoSql documentation, including the full grammar.
