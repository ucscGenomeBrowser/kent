table bed5Pval
"Scored BED with P-value floating point values and integer scores (0-1000) for display."
    (
    string chrom;      "Reference sequence chromosome or scaffold"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of item"
    int    score;      "0-1000 score for useScore shading"
    float  pValue;     "-log10 P-value"
    )
