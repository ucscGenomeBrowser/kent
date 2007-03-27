table kg1ToKg2
"Lists old genes and equivalent new genes if any, and reason for dropping if any."
    (
    string oldId;	"Old gene ID"
    string oldChrom;	"Old chromosome "
    int oldStart;	"Old start position"
    int oldEnd;		"Old end position"
    string newId;	"New gene ID, may be empty"
    string status;	"How new/old correspond. Exact and compatible are good. Overlap, none are bad."
    string note;	"Additional info if status if overlap or none."
    )
