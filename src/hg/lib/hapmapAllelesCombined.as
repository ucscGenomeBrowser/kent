table hapmapAllelesCombined
"HapMap allele counts by population"
    (
    string  chrom;      "Chromosome"
    uint    chromStart; "Start position in chrom (0 based)"
    uint    chromEnd;   "End position in chrom (1 based)"
    string  name;       "Reference SNP identifier from dbSnp"
    ushort  score;      "Use for heterozygosity"
    char[1] strand;     "Which genomic strand contains the observed alleles"

    string  observed;   "Observed string from genotype file"

    char[1] allele1;    "This allele has been observed"
    ushort  allele1CountCEU;       "allele1 count for the CEU population"
    ushort  allele1CountCHB;       "allele1 count for the CHB population"
    ushort  allele1CountJPT;       "allele1 count for the JPT population"
    ushort  allele1CountYRI;       "allele1 count for the YRI population"

    string  allele2;    "This allele may not have been observed"
    ushort  allele2CountCEU;       "allele2 count for the CEU population"
    ushort  allele2CountCHB;       "allele2 count for the CHB population"
    ushort  allele2CountJPT;       "allele2 count for the JPT population"
    ushort  allele2CountYRI;       "allele2 count for the YRI population"

    ushort  heteroCountCEU;        "Count of CEU individuals who are heterozygous"
    ushort  heteroCountCHB;        "Count of CHB individuals who are heterozygous"
    ushort  heteroCountJPT;        "Count of JPT individuals who are heterozygous"
    ushort  heteroCountYRI;        "Count of YRI individuals who are heterozygous"

    )
