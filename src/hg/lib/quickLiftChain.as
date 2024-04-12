table quickLiftChain
"bigChain files used by quickLift. */
    (
    uint id;            " Auto-incrementing ID"
    string  fromDb;	"Short name of 'from' database.  'hg15' or the like"
    string  toDb;	"Short name of 'to' database.  'hg16' or the like"
    lstring path;	"Path to bigChain file"
    )
