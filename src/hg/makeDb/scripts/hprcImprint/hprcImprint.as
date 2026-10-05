table hprcImprintDmr
"Differentially methylated region between maternal and paternal haplotypes"
    (
    string chrom;      "Chromosome"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Region identifier"
    uint   score;      "Not used"
    char[1] strand;    "Strand, not used"
    uint   thickStart; "Start of thick part"
    uint   thickEnd;   "End of thick part"
    uint   reserved;   "Color of the region"
    string parent;     "Higher methylation on|Haplotype with the higher methylation level"
    float  methDiff;   "Methylation difference|Maternal minus paternal methylation level"
    )
