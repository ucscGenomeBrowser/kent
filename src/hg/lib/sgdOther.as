table sgdOther
"Features other than coding genes from yeast genome database"
    (
    string chrom;	"Chromosome in chrNN format"
    int chromStart;	"Start (zero based)"
    int chromEnd;	"End (non-inclusive)"
    string name;	"Feature name"
    char[1] strand;	"Strand: +, - or ."
    string type;	"Feature type"
    )
