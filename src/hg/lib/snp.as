table snp
"SNP positions from various sources"
    (
    string  chrom;	"Chromosome or 'unknown'"
    uint    chromStart; "Start position in chrom"
    uint    chromEnd;	"End position in chrom"
    string  name;	"Name of SNP - rsId or Affy name"
    float   score;      "certainty of variation"
    char[1] strand;     "+ or -"
    string  alleles;   	"the sequence of the observed alleles"
    string  source;	"BAC_OVERLAP | MIXED | RANDOM | OTHER | Affy10K | Affy120K"
    string  class;      "SNP | INDEL | SEGMENTAL"
    string  valid;   	"the validation status of the SNP"
    float   avHet;   	"the average heterozygosity from all observations"
    float   avHetSE; 	"the Standard Error for the average heterozygosity"
    string  func; 	"the functional category of the SNP, if any"
    )
