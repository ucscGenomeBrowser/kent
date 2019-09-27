table bed3Sources
"BED3+2 with a count and list of sources for combined data"
    (
    string chrom;      "Reference sequence chromosome or scaffold"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    uint sourceCount;  "Number of sources"
    uint[sourceCount] sourceIds; "Source ids"
    )
