table RnaElements 
"BED6 + 3 scores for RNA Elements data "
    (
    string chrom;      "Chromosome (or contig, scaffold, etc.)"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of item"
    uint   score;      "Normalized score from 0-1000"
    char[1] strand;    "+ or -"
    float level;       "Expression level such as RPKM or FPKM"
    float signif;      "Statistical significance such as IDR"
    uint score2;       "Additional measurement/count e.g. number of reads"
    )
