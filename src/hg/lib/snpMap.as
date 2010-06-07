table snpMap
"SNP positions from various sources"
    (
    string chrom;	"Reference sequence chromosome or scaffold"
    uint   chromStart;  "Start position in chrom"
    uint   chromEnd;	"End position in chrom"
    string name;	"Name of SNP - rsId or Affy name"
    string source;	"BAC_OVERLAP | MIXED | RANDOM | OTHER | Affy10K | Affy120K"
    string type;        "SNP | INDEL | SEGMENTAL"
    )
