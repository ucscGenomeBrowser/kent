table barChartGeneExp
"BED6+5 with additional fields for category count and median values, and sample matrix fields"
    (
    string chrom;       "Reference sequence chromosome or scaffold"
    uint   chromStart;  "Start position in chromosome"
    uint   chromEnd;    "End position in chromosome"
    string name;  "Gene identifier"
    uint   score;       "Score from 0-1000, typically derived from total of median value from all categories"
    char[1] strand;     "+ or - for strand. Use . if not applicable"
    string name2;    "Hugo gene name"
    uint expCount;      "Number of categories"
    float[expCount] expScores; "Comma separated list of category values"
    bigint _dataOffset; "Offset of sample data in data matrix file, for boxplot on details page"
    int _dataLen;       "Length of sample data row in data matrix file"
    )
