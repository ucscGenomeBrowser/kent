table itemAttr
"Relational object of display attributes for individual items"
    (
    string chrom;      "chromosome"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    uint   itemId;     "Primary key of item"
    ubyte colorR;      "Color red component 0-255"
    ubyte colorG;      "Color green component 0-255"
    ubyte colorB;      "Color blue component 0-255"
    )
