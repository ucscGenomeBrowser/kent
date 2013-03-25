table ceBlastTab
"Blast alignments of C. elegans genes"
    (
    string query;	"Name of sequence in this assembly (query)"
    string target;	"Name of sequence in other assembly (target)"
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

