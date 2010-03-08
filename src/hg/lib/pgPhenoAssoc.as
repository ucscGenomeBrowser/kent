table pgPhenoAssoc
"phenotypes from various databases for pgSnp tracks"
    (
    string  chrom;          "Chromosome"
    uint    chromStart;     "Start position in chrom"
    uint    chromEnd;       "End position in chrom"
    string  name;           "Phenotype"
    lstring srcUrl;	    "link back to source database
    )
