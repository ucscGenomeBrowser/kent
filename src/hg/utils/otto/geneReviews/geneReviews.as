table geneReviews
"GeneReviews gene location and diseases.  BED 9+2.
    (
    string chrom;      "Reference sequence chromosome or scaffold"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Gene symbol"
    uint   score;      "Score from 0-1000"
    char[1] strand;    "."
    uint thickStart;   "Start position"
    uint thickEnd;     "End position"
    uint reserved;     "Used as itemRgb as of 2004-11-22"
    uint diseaseCount; "Count of related diseases"
    string diseases;  "List of related diseases (semi-colon separated), for display"
    )
