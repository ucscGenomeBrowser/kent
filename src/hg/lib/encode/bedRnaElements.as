table RnaElements 
"BED6 + 3 scores for RNA Elements data "
    (
    string chrom;      "Reference sequence chromosome or scaffold"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of item"
    uint   score;      "Normalized score from 0-1000"
    char[1] strand;    "+ or - or . for unknown"
    float level;       "Expression level such as RPKM or FPKM. Set to -1 for no data."
    float signif;      "Statistical significance such as IDR. Set to -1 for no data."
    uint score2;       "Additional measurement/count e.g. number of reads. Set to 0 for no data."
    )
