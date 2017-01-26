table barChartBed
"BED6+ with additional fields for category count and values
    (
    string chrom;       "Reference sequence chromosome or scaffold"
    uint   chromStart;  "Start position in chromosome"
    uint   chromEnd;    "End position in chromosome"
    string name;        "Item identifier"
    uint   score;       "Score from 0-1000; derived from total median all categories (log-transformed and scaled)"
    char[1] strand;     "+ or - for strand"
    uint expCount;      "Number of categories"
    float[expCount] expScores; "Comma separated list of category values"
    )
