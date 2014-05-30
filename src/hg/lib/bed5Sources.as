table bed5Sources
"BED5+ with a float data value field and a list of sources for combined data"
    (
    string chrom;      "Reference sequence chromosome or scaffold"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of item"
    uint   score;      "Display score (0-1000)"
    float  floatScore; "Data value"
    uint sourceCount;   "Number of sources"
    uint[sourceCount] sourceIds; "Source ids"
    )
