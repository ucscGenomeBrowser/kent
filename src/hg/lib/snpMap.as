table snpMap
"SNP positions from various sources"
    (
    string chrom;	"Chromosome or 'unknown'"
    int    chromStart;  "Start position in chrom - negative 1 if unpositioned"
    uint   chromEnd;	"End position in chrom"
    string name;	"Name of SNP"
    string source;	"BAC_OVERLAP | MIXED | RANDOM | OTHER"
    string type;        "SNP | INDEL | SEGMENTAL"
    )
