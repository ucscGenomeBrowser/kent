table bed6FloatVal
"BED 6 (with 0-1000 score), but also with floating-point value."
    (
    string chrom;      "Reference sequence chromosome or scaffold"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of item"
    int    score;      "0-1000 score for useScore shading"
    char[1] strand;    "+ or -"
    float  val;         "Floating point value."
    )

