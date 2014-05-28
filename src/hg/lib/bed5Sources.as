table bed5Sources
"BED5+ with a float data value field and a list of sources for combined data"
    (
    string chrom;      "Reference sequence chromosome or scaffold"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of item"
    uint   score;      "Display score (0-1000)"
    float  floatScore; "Data score (signal, p-value, z-score, etc.)"
    uint sourceCount;   "Number of sources contributing to this item"
    uint[sourceCount] sourceIds; "Comma separated list of source ids"
    )
