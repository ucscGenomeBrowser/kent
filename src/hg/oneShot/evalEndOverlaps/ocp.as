table ocp
"Information on overlapping clone pair"
    (
    string aName;	"Name of first clone"
    int aSize;		"Size of first clone"
    char[1] aHitS;	"Value Y, N and ? for corresponding end hitting, not hitting and unknown"
    char[1] aHitT;	"Value Y, N and ? for corresponding end hitting, not hitting and unknown"
    string bName;	"Name of second clone"
    int bSize;		"Size of second clone"
    char[1] bHitS;	"Value Y, N and ? for corresponding end hitting, not hitting and unknown"
    char[1] bHitT;	"Value Y, N and ? for corresponding end hitting, not hitting and unknown"
    int overlap;	"Bases of overlap"
    int reserved;	"Reserved (always 0 now)"
    )
