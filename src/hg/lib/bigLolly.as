table refSeqFuncElems
"Bed 9+"
    (
    string chrom;          "Reference sequence chromosome or scaffold"
    uint   chromStart;     "Start position in chromosome"
    uint   chromEnd;       "End position in chromosome"
    string name;           "type of element"
    uint   score;          "unused; placeholder for BED format"
    char[1] strand;        "+ for forward strand, - for reverse strand"
    uint   thickStart;     "Start position in chromosome"
    uint   thickEnd;       "End position in chromosome"
    uint reserved;         "Used as itemRgb: color based on type of element"
    lstring _mouseOver;    "Mouse over label"
    )
