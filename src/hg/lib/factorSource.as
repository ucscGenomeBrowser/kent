table factorSource
"BED5+ with two fields having variable number of experiment IDs and values (none zero-valued)"
    (
    string chrom;      "Reference sequence chromosome or scaffold"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of item"
    uint   score;      "Score from 0-1000"
    uint expCount;     "Number of experiment values"
    uint[expCount] expNums; "Comma separated list of experiment numbers"
    float[expCount] expScores; "Comma separated list of experiment scores"
    )
