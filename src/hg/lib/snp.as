table snp
"SNP positions from various sources"
    (
    string  chrom;      "Chromosome"
    uint    chromStart; "Start position in chrom"
    uint    chromEnd;   "End position in chrom"
    string  name;       "Name of SNP: rsId or Affy name"
    float   score;      "Certainty of variation"
    char[1] strand;     "Which DNA strand contains the observed alleles"
    string  alleles;    "The sequence of the observed alleles"
    string  source;     "How the variant was discovered"
    string  class;      "The class of variant"
    string  valid;      "The validation status of the SNP"
    float   avHet;      "The average heterozygosity from all observations"
    float   avHetSE;    "The Standard Error for the average heterozygosity"
    string  func;       "The functional category of the SNP, if any"
    )
