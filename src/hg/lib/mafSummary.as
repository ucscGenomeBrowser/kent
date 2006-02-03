table mafSummary
"Positions and scores for alignment blocks"
    (
    string chrom;      "Reference sequence chromosome or scaffold"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string src;        "Sequence name or database of alignment"
    float  score;      "Floating point score."
    char[1] leftStatus;  "Gap/break annotation for preceding block"
    char[1] rightStatus; "Gap/break annotation for following block"
    )
