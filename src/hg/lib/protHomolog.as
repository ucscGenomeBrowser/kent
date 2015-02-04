table protHomolog
"table to store SAM homolog results"
    (
    string  proteinID;  "protein ID"
    string  homologID;  "homolog ID"
    char[1] charin;	"chain"
    int     length;	"length of protein sequence"
    double  bestEvalue; "best E value"
    double  evalue;	"E value"
    string  FSSP;	"FSSP ID"
    string  SCOPdomain; "SCOP domain ID"
    string  SCOPsuid;	"SCOP sunid"
    )
