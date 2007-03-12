table hapmapAllelesCombined
"HapMap allele counts by population"
    (
    string  chrom;      "Chromosome"
    uint    chromStart; "Start position in chrom (0 based)"
    uint    chromEnd;   "End position in chrom (1 based)"
    string  name;       "Reference SNP identifier from dbSnp"
    uint  score;      "Use for heterozygosity"
    char[1] strand;     "Which genomic strand contains the observed alleles"

    string  observed;   "Observed string from genotype file"

    char[1] allele1;    "This allele has been observed"
    uint  allele1CountCEU;       "allele1 count for the CEU population"
    uint  allele1CountCHB;       "allele1 count for the CHB population"
    uint  allele1CountJPT;       "allele1 count for the JPT population"
    uint  allele1CountYRI;       "allele1 count for the YRI population"

    string  allele2;    "This allele may not have been observed"
    uint  allele2CountCEU;       "allele2 count for the CEU population"
    uint  allele2CountCHB;       "allele2 count for the CHB population"
    uint  allele2CountJPT;       "allele2 count for the JPT population"
    uint  allele2CountYRI;       "allele2 count for the YRI population"

    uint  heteroCountCEU;        "Count of CEU individuals who are heterozygous"
    uint  heteroCountCHB;        "Count of CHB individuals who are heterozygous"
    uint  heteroCountJPT;        "Count of JPT individuals who are heterozygous"
    uint  heteroCountYRI;        "Count of YRI individuals who are heterozygous"

    )
