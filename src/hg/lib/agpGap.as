table agpGap
"Gaps in golden path"
    (
    string chrom;	"which chromosome"
    uint chromStart;	"start position in chromosome"
    uint chromEnd;	"end position in chromosome"
    int ix;             "ix of this fragment (useless)"
    char[1] n;          "always 'N'"
    uint size;          "size of gap"
    string type;        "contig, clone, fragment, etc."
    string bridge;      "yes, no, mrna, bacEndPair, etc."
    )

