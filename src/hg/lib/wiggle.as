table wiggle
"Wiggle track values to display as y-values (first 6 fields are bed6)"
(
string chrom;         "Human chromosome or FPC contig"
uint chromStart;      "Start position in chromosome"
uint chromEnd;        "End position in chromosome"
string name;          "Name of item"
uint   score;         "Score from 0-1000 currently meaningless"
char[1] strand;       "+ or - (may be not needed for this)"
uint Span;            "each value spans this many bases"
uint Count;           "number of values to use"
uint Offset;          "offset in File to fetch data"
string File;          "path name to data file, one byte per value"
)

