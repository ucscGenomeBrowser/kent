
table hubPublic
"Table of public track data hub connections."
    (
    lstring hubUrl; "URL to hub.ra file"
    string shortLabel;	"Hub short label."
    string longLabel;	"Hub long label."
    string registrationTime; "Time first registered"
    uint dbCount;	"Number of databases hub has data for."
    string dbList; "Comma separated list of databases."
    lstring descriptionUrl; "URL to description HTML"
    )
