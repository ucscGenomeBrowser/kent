table bed5FloatScore
"BED 5 (with 0-1000 score), but also with floating-point score."
    (
    string chrom;      "Reference sequence chromosome or scaffold"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of item"
    int    score;      "0-1000 score for useScore shading"
    float  floatScore; "Floating point score."
    )
