table haploinsufficiency
"Haploinsufficiency data in BED 9+2 format."
    (
    string chrom;      "Chromosome (or contig, scaffold, etc.)"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of item"
    uint   score;      "Score from 0-1000"
    char[1] strand;    "+ or -"
    uint thickStart;   "Start of where display should be thick (start codon)"
    uint thickEnd;     "End of where display should be thick (stop codon)"
    uint itemRgb;      "RGB value for color of item"
    string quantile;   "Alpha-upper quantile of the predicted score (%HI)"
    float prediction;  "Predicted score for haploinsufficiency of the item"
    )
