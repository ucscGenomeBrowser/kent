table bed5SourceVals
"BED5+ with a count, list of sources, and list of source scores for combined data"
    (
    string chrom;      "Reference sequence chromosome or scaffold"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of item"
    uint   score;      "Display score (0-1000)"
    uint sourceCount;   "Number of sources"
    uint[sourceCount] sourceIds; "Source ids"
    float[sourceCount] sourceScores; "Source scores"
    )
