table hapmapSnpsCombined
"HapMap genotype summaries for all populations"
    (
    string  chrom;      "Chromosome"
    uint    chromStart; "Start position in chrom (0 based)"
    uint    chromEnd;   "End position in chrom (1 based)"
    string  name;       "Reference SNP identifier from dbSnp"
    uint  score;        "Not used"
    char[1] strand;     "Which genomic strand contains the observed alleles"

    string  observed;   "Observed string from genotype file"

    char[1] allele1;    "This allele has been observed"
    uint  homoCount1CEU;	"Count of CEU individuals who are homozygous for allele1"
    uint  homoCount1CHB;	"Count of CHB individuals who are homozygous for allele1"
    uint  homoCount1JPT;	"Count of JPT individuals who are homozygous for allele1"
    uint  homoCount1YRI;	"Count of YRI individuals who are homozygous for allele1"

    string  allele2;    "This allele may not have been observed"
    uint  homoCount2CEU;	"Count of CEU individuals who are homozygous for allele2"
    uint  homoCount2CHB;	"Count of CHB individuals who are homozygous for allele2"
    uint  homoCount2JPT;	"Count of JPT individuals who are homozygous for allele2"
    uint  homoCount2YRI;	"Count of YRI individuals who are homozygous for allele2"

    uint  heteroCountCEU;        "Count of CEU individuals who are heterozygous"
    uint  heteroCountCHB;        "Count of CHB individuals who are heterozygous"
    uint  heteroCountJPT;        "Count of JPT individuals who are heterozygous"
    uint  heteroCountYRI;        "Count of YRI individuals who are heterozygous"

    )
