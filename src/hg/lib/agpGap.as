table agpGap
"Gaps in golden path"
    (
    string chrom;	"Reference sequence chromosome or scaffold"
    uint chromStart;	"start position in chromosome"
    uint chromEnd;	"end position in chromosome"
    int ix;             "index count of this fragment (obsolete/useless)"
    char[1] n;          "'N' for gaps of known size, 'U' for gaps of unknown size"
    uint size;          "size of gap"
    string type;        "scaffold, contig, clone, fragment, etc."
    string bridge;      "yes, no, mrna, bacEndPair, etc."
    )
