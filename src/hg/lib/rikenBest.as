table rikenBest
"The best Riken mRNA in each cluster"
    (
    string name;	"Riken mRNA ID"
    float orfScore; "Score from bestorf - 50 is decent"
    char[1] orfStrand; "Strand bestorf is on: +, - or ."
    int intronOrientation;  "+1 for each GT/AG intron, -1 for each CT/AC"
    string position;	"Position in genome of cluster chrN:start-end format"
    int rikenCount;	"Number of Riken mRNAs in cluster"
    int genBankCount;	"Number of Genbank mRNAs in cluster"
    int refSeqCount;    "Number of RefSeq mRNAs in cluster"
    string clusterId;	"ID of cluster"
    )
