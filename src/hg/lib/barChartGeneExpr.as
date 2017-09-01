table barChartGeneExpr
"BED6+5 barChart format, with name, name2 identifying gene name and accession"
    (
    string chrom;       "Reference sequence chromosome or scaffold"
    uint   chromStart;  "Start position in chromosome"
    uint   chromEnd;    "End position in chromosome"
    string name;        "Accession"
    uint   score;       "Score from 0-1000, typically derived from total of median value from all tissues"
    char[1] strand;     "+ or - for strand. Use . if not applicable"
    string name2;       "HUGO gene name"
    uint expCount;      "Number of tissues"
    float[expCount] expScores; "Comma separated list of median expression values per tissue"
    bigint _dataOffset; "Offset of sample data in data matrix file, for boxplot on details page"
    int _dataLen;       "Length of sample data row in data matrix file"
    )
