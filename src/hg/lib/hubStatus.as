
table hubStatus
"Table of track data hub connection status."
    (
    uint id;	"Auto-incrementing hub ID"
    lstring hubUrl; "URL to hub.ra file"
    string shortLabel;	"Hub short label."
    string longLabel;	"Hub long label."
    uint dbCount;	"Number of databases hub has data for."
    lstring dbList; "Comma separated list of databases."
    uint status;        "1 if private"
    string lastOkTime;	"Time when hub last was ok"
    string lastNotOkTime; "Time when hub last was not ok"
    lstring errorMessage; "If non-empty contains last error message from hub. If empty hub is ok."
    string firstAdded;   "Time when hub was first added"
    )
