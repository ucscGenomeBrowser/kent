table itemAttr
"Relational object of display attributes for individual track items"
    (
    string name;       "name of item"
    string chrom;      "Reference sequence chromosome or scaffold"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    ubyte colorR;      "Color red component 0-255"
    ubyte colorG;      "Color green component 0-255"
    ubyte colorB;      "Color blue component 0-255"
    )
