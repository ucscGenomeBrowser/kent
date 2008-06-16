table pgSnp
"personal genome SNP"
    (
    ushort  bin;            "A field to speed indexing"
    string  chrom;          "Chromosome"
    uint    chromStart;     "Start position in chrom"
    uint    chromEnd;       "End position in chrom"
    string  name;           "alleles"
    int     alleleCount;    "number of alleles"
    string  alleleFreq;     "comma separated list of frequency of each allele"
    )

