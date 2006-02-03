table pileups
"Pileup locations and hit count min/avg/max."
    (
    string chrom;      "Reference sequence chromosome or scaffold"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name (pileup location suitable for browser position)"
    uint   score;      "Int score (bed compat) -- use average by default."
    uint   pMin;       "Minimum hit count of any base in this pileup."
    float  pAvg;       "Average hit count for all bases in this pileup."
    uint   pMax;       "Maximum hit count of any base in this pileup."
    )
