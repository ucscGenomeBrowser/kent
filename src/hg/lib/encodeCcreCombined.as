table encodeCcreCombined
"Cell-type agnostic cCRE from ENCODE. BED 9+6."
(
string  chrom;		"Reference sequence chromosome or scaffold"
uint    chromStart;	"Start position of feature on chromosome"
uint    chromEnd;	"End position of feature on chromosome"
string  name;	        "ENCODE accession"
uint    score;		"0-1000, based on Z score"
char[1] strand;		"n/a"
uint    thickStart;	"Start position"
uint    thickEnd;	"End position"
uint  	reserved;       "RGB Color"
string  ccre;	        "ENCODE classification"
string  encodeLabel;    "ENCODE label"
float   zScore;         "Max DNase Z-score"
string  ucscLabel;      "UCSC label"
string  accessionLabel; "Accession (abbreviated)"
string  description;    "Description for browser hover"
)
