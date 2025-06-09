table clsTargetBed
"CLS target regions"
    (
    string chrom;      "Chromosome"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Target number"
    uint   score;      "unused"
    char[1] strand;    "+ or -"
    int targetIdCnt;  "Number of target ids for this merged region"
    string[targetIdCnt] targetIds;  "Targets ids that are part of this region"
    )
