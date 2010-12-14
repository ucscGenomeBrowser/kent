
table hubConnect
"Table of track data hub connections."
    (
    uint id;	"Auto-incrementing hub ID"
    string shortLabel;	"Hub short label."
    string longLabel;	"Hub long label."
    lstring hubUrl; "URL to hub.ra file"
    string registrationTime; "Time first registered"
    string lastOkTime;	"Time when hub last was ok"
    string lastNotOkTime; "Time when hub last was not ok"
    lstring errorMessage; "If non-empty contains last error message from hub. If empty hub is ok."
    uint dbCount;	"Number of databases hub has data for."
    string dbList[dbCount]; "Comma separated list of databases."
    )
