table snpTmp
"Polymorphism data subset used during processing"
    (
    string  chrom;      "Chromosome"
    uint    chromStart; "Start position in chrom"
    uint    chromEnd;   "End position in chrom"
    string  name;       "Reference SNP identifier or Affy SNP name"
    char[1] strand;     "Which DNA strand contains the observed alleles"
    lstring  refNCBI;  	"Reference genomic from dbSNP"
    string  locType;    "range, exact, between"
    )
