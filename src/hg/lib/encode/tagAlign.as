table tagAlign
"Tag Alignment format (BED 3+)"
(
    string chrom;        "Reference sequence chromosome or scaffold"
    uint   chromStart;   "Start position in chromosome"
    uint   chromEnd;     "End position in chromosome"
    string sequence;     "Sequence of this read"
    int    score;        "Indicates mismatches, quality, or other measurement (0-1000)"
    char   strand;       "Orientation of this read (+ or -)"
)
