table recount3
"Bed 9+6 file for NCBI orthologs"
    (
    string chrom;      "Reference sequence chromosome or scaffold"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Short Name of item"
    uint   score;      "Score from 0-1000"
    char[1] strand;    "+ or -"
    uint thickStart;   "Start of where display should be thick"
    uint thickEnd;     "End of where display should be thick"
    uint reserved;     "Used as itemRgb as of 2004-11-22"
    bigint readcount;  "Read count"
    uint samplecount;  "Sample count"
    string donor;      "Splice donor"
    string acceptor;   "Splice acceptor"
    string url;        "URL"
    )
