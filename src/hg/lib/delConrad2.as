table delConrad2
"Deletions from Conrad"
    (
    string chrom;	"Reference sequence chromosome or scaffold"
    uint   chromStart;	"Start position in chrom"
    uint   chromEnd;	"End position in chrom"
    string name;	"Name"
    uint   score;	"Always 1000"
    char[1] strand;	"Always positive"
    uint   thickStart;  "Max start (can be different from chromStart)"
    uint   thickEnd;    "Max end (can be different from chromEnd)"
    uint   count1;      "Count1"
    uint   count2;      "Count2"
    string offspring;   "HapMap identifier"
    string population;  "HapMap population"
    )
