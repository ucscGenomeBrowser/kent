table pushQ
"Push Queue"
    (
    char [6]  qid;       "Queue Id"
    char [6]  pqid;       "Parent Queue Id if split off another"
    char [1]  priority;   "Priority"
    uint      rank;       "Rank for display sort"
    char [10] qadate;     "QA start date"
    char [1]  newYN;      "new (track)?"
    string    track;      "Track"
    string    dbs;        "Databases"
    lstring   tbls;       "Tables"
    string    cgis;       "CGI(s)"
    string    files;      "File(s)"
    uint      sizeMB;     "Size MB"
    char [20] currLoc;    "Current Location"
    char [1]  makeDocYN;  "MakeDoc verified?"
    char [50] onlineHelp; "Online Help"
    char [1]  ndxYN;      "Index verified?"
    char [1]  joinerYN;   "all.joiner verified?"
    string    stat;       "Status"
    char [50] sponsor;    "Sponsor"
    char [50] reviewer;   "QA Reviewer"
    char [50] extSource;  "External Source"
    lstring   openIssues; "Open issues"
    lstring   notes;      "Notes"
    char [10] reqdate;    "Push-request Date"
    char [1]  pushedYN;   "Push done?"
    )

table users
"PushQ Users"
    (
    char[8]  user;        "User"
    char[34] password;    "Password" 
    char[8]  role;        "Role=admin,dev,qa"
	char[20] cacheDefeat; "Random string to defeat caches"
	lstring  contents;    "pushq-cart contents"
    )
