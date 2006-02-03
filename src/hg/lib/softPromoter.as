table softPromoter
"Softberry's Promoter Predictions"
    (
    string chrom;      	"Reference sequence chromosome or scaffold"
    uint   chromStart; 	"Start position in chromosome"
    uint   chromEnd;   	"End position in chromosome"
    string name;       	"As displayed in browser"
    uint   score;        "Score from 0 to 1000"
    string type;	"TATA+ or TATAless currently"
    float origScore;    "Score in original file, not scaled"
    string origName;    "Name in original file"
    string blockString; "From original file.  I'm not sure how to interpret."
    )
