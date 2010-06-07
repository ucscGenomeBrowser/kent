table sgdOther
"Features other than coding genes from yeast genome database"
    (
    string chrom;	"Reference sequence chromosome or scaffold"
    int chromStart;	"Start (zero based)"
    int chromEnd;	"End (non-inclusive)"
    string name;	"Feature name"
    int score;		"Always 0"
    char[1] strand;	"Strand: +, - or ."
    string type;	"Feature type"
    )
