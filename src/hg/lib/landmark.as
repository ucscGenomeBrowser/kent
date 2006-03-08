table landmark
"track for landmarks from locus experts"
    (
    ushort  bin;            "A field to speed indexing"
    string  chrom;          "Chromosome"
    uint    chromStart;     "Start position in chrom"
    uint    chromEnd;       "End position in chrom"
    string  name;           "name of landmark"
    ushort  srcId;          "key into hgMutSrc table"
    string  regionType;    "type of landmark: HSS, promoter, .."
    )

