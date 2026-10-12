table monk2018Imprint
"Imprinted differentially methylated regions relevant for clinical diagnostics (Monk et al 2018)"
    (
    string chrom;      "Chromosome"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Imprinted DMR name"
    uint   score;      "Not used"
    char[1] strand;    "Strand, not used"
    uint   thickStart; "Start of thick part"
    uint   thickEnd;   "End of thick part"
    uint   reserved;   "Color of the region"
    string methylationOrigin; "Methylation origin|Parent whose allele is methylated"
    uint   numCpGs;    "Number of CpGs|CpG sites in the DMR as defined by methyl-seq data"
    string germlineDerived; "Germline derived|Whether the methylation is established in the germline (oocyte or sperm gDMR) or later"
    string lrgId;      "LRG identifier|Locus Reference Genomic record covering the DMR"
    string aliases;    "Aliases|Other names used for the DMR"
    string hg19Pos;    "Original position (hg19)|Coordinates in the paper, before the lift to hg38"
    )
