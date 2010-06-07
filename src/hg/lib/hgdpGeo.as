table hgdpGeo
"Human Genome Diversity Project population allele frequencies (bed 4+)"
    (
    string    chrom;           "Reference sequence chromosome or scaffold"
    uint      chromStart;      "Start position in chromosome"
    uint      chromEnd;        "End position in chromosome"
    string    name;            "SNP ID (dbSNP reference SNP rsNNNNN ID)"
    char      ancestralAllele; "Ancestral allele on forward strand of reference genome assembly"
    char      derivedAllele;   "Derived allele on forward strand of reference genome assembly"
    float[53] popFreqs;        "For each population in alphabetical order, the proportion of the population carrying the ancestral allele."
    )
