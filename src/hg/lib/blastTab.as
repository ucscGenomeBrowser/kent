table blastTab
"Tab-delimited blast output file"
    (
    string query;	"Name of query sequence"
    string target;	"Name of target sequence"
    float identity;	"Percent identity"
    uint aliLength;	"Length of alignment"
    uint mismatch;	"Number of mismatches"
    uint gapOpen;	"Number of gap openings"
    uint qStart;	"Start in query (0 based)"
    uint qEnd;		"End in query (non-inclusive)"
    uint tStart;	"Start in target (0 based)"
    uint tEnd;		"End in target (non-inclusive)"
    double eValue;	"Expectation value"
    double bitScore;	"Bit score"
    )

