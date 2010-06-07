table contrastGene
"CONTRAST gene prediction"
    (
    string chrom;      "Reference sequence chromosome or scaffold"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Generate name of prediction"
    uint   score;      "not used, always 1000"
    char[1] strand;    "+ or -"
    uint thickStart;   "Start of CDS"
    uint thickEnd;     "End of CDS"
    uint reserved;     "Unused"
    int blockCount;    "Number of exons"
    int[blockCount] blockSizes; "List of exon sizes"
    int[blockCount] chromStarts; "Start positions of exons relative to chromStart"
    uint expCount;     "Number of exons (same as blockCount)"
    int[expCount] expIds; "List of colors for exons, indicate confidence"
    )
