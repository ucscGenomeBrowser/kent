table hapmapAllelesSummary
"HapMap allele summaries for filtering"
    (
    string  chrom;      "Chromosome"
    uint    chromStart; "Start position in chrom (0 based)"
    uint    chromEnd;   "End position in chrom (1 based)"
    string  name;       "Reference SNP identifier from dbSnp"
    uint    score;      "Use for heterozygosity"
    char[1] strand;     "Which genomic strand contains the observed alleles"

    string  observed;   "Observed string from genotype file"
    char[1] allele1;    "This allele has been observed"
    string  allele2;    "This allele may not have been observed"
    uint    popCount;   "How many populations have data"
    string  isMixed;    "Are there different major alleles?"
    
    string  majorAlleleCEU;        "major allele for the CEU population"
    uint    majorAlleleCountCEU;   "major allele count for the CEU population"
    uint    totalAlleleCountCEU;   "total allele count for the CEU population"
    string  majorAlleleCHB;        "major allele for the CHB population"
    uint    majorAlleleCountCHB;   "major allele count for the CHB population"
    uint    totalAlleleCountCHB;   "total allele count for the CHB population"
    string  majorAlleleJPT;        "major allele for the JPT population"
    uint    majorAlleleCountJPT;   "major allele count for the JPT population"
    uint    totalAlleleCountJPT;   "total allele count for the JPT population"
    string  majorAlleleYRI;        "major allele for the YRI population"
    uint    majorAlleleCountYRI;   "major allele count for the YRI population"
    uint    totalAlleleCountYRI;   "total allele count for the YRI population"

    string  chimpAllele;           "chimp allele"
    uint    chimpAlleleQuality;    "quality score (0-100)"
    string  macaqueAllele;         "macaque allele"
    uint    macaqueAlleleQuality;  "quality score (0-100)"

    )
