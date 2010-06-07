table history
"Who loaded what tables when"
    (
    int ix;		"auto_increment ID"
    uint startId;	"First extFile index affected by this action (or 0)"
    uint endId;		"Last extFile index affected by this action (or 0)"
    string who;		"Unix userID that performed this action"
    string what;	"Brief description of what table(s) were loaded"
    string modTime;	"Time of action"
    string errata;	"Any after-the-fact comments about changes"
    )
