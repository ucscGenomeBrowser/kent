table barChartBedPlus
"bed6+5 plus additional fields"
    (
    string chrom;               "Reference sequence chromosome or scaffold"
    uint chromStart;            "Start position in chromosome"
    uint chromEnd;              "End position in chromosome"
    string name;                "Name or ID of item"
    uint score;                 "Score (0-1000)"
    char[1] strand;             "'+','-' or '.'. Indicates whether the query aligns to the + or - strand on the reference"
    string name2;               "Alternate name of item"
    uint expCount;              "Number of bar graphs in display, must be <= 100"
    float[expCount] expScores;  "Comma separated list of category values."
    bigint _dataOffset;         "Offset of sample data in data matrix file, for boxplot on details page, optional only for barChart format"
    int _dataLen;               "Length of sample data row in data matrix file, optional only for barChart format"
    string extra1;              "extra field one"
    string extra2;              "extra field two"
    )
