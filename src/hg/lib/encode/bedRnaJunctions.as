table RnaJunctions
"BED6 + 3 scores for RNA Splice Junctions data "
    (
    string chrom;      "Reference sequence chromosome or scaffold"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of item"
    uint   score;      "Normalized score from 0-1000"
    char[1] strand;    "+ or - or . for unknown"
    uint total ;       "Number of fragments crossing the junctions"
    float npIDR;       "Non-paramteric IDR (use -1 if no IDR run)"
    uint staggered;    "Number of distinct fragments crossing the junctions, i.e. all read pairs for which the beginnings of both reads are the same are collapsed into one"
    )
