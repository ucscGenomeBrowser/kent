table hapmapSnpsCEU
"HapMap genotype summary"
    (
    string  chrom;      "Chromosome"
    uint    chromStart; "Start position in chrom (0 based)"
    uint    chromEnd;   "End position in chrom (1 based)"
    string  name;       "Reference SNP identifier from dbSnp"
    uint    score;      "Minor allele frequency normalized (0-500)"
    char[1] strand;     "Which genomic strand contains the observed alleles"

    string  observed;   "Observed string from genotype file"

    char[1] allele1;    "This allele has been observed"
    uint    homoCount1;       "Count of individuals who are homozygous for allele1"

    string  allele2;    "This allele may not have been observed"
    uint    homoCount2;       "Count of individuals who are homozygous for allele2"

    uint    heteroCount;   "Count of individuals who are heterozygous"
    )

