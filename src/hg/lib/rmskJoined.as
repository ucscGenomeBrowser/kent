table rmskJoined
"RepeatMasker joined annotation record"
    (
    string chrom;      "Reference sequence chromosome or scaffold"
    uint   chromStart; "Start position of feature in chromosome"
    uint   chromEnd;   "End position feature in chromosome"
    string name;       "Short Name of item"
    uint   score;      "Score from 0-1000"
    char[1] strand;    "+ or -"
    uint alignStart;   "Start position of aligned portion of feature"
    uint alignEnd;     "End position of aligned portion of feature"
    uint reserved;     "Used as itemRgb"
    int blockCount;    "Number of blocks"
    int[blockCount] blockSizes; "Comma separated list of block sizes"
    int[blockCount] blockRelStarts; "Start positions rel. to chromStart or -1 for unaligned blocks"
    string id;         "ID to bed used in URL to link back"
    lstring description; "Long description of item for the details page"
    )
