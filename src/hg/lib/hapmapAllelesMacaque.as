table hapmapAllelesMacaque
"Ortho alleles for HapMap SNPs"
    (
    string  chrom;       "Chromosome"
    uint    chromStart;  "Start position in chrom (0 based)"
    uint    chromEnd;    "End position in chrom (1 based)"
    string  name;        "Reference SNP identifier from dbSnp"
    uint    score;       "Quality score"
    char[1] strand;      "Which genomic strand contains the observed alleles"
    lstring refUCSC;     "Reference genomic"
    string  observed;    "dbSNP polymorphism"

    string  orthoChrom;  "Chromosome in other org"
    uint    orthoStart;  "Start position in other org"
    uint    orthoEnd;    "End position in other org"
    char[1] orthoStrand; "Strand in other org"
    char[1] orthoAllele; "Allele in other org"

    )

