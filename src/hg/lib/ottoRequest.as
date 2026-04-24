table ottoRequest
"recording requests coming from from functions such as liftRequest"
    (
    uint id;	      "Auto-incrementing request count"
    string requestType;  "type of request: liftOver or assembly"
    string fromDb;    "can be a database name or a GC[AF]_ GenArk accession"
    string toDb;      "can be a database name or a GC[AF]_ GenArk accession"
    string email;     "user email address"
    lstring comment;   "other comments from the input form"
    string requestTime; "date time request was added"
    ubyte doneStatus;   "0 == pending, 1 == notified/in progress, 2 == complete, 3 == problems"
    string buildDir;	"build directory path for alignment workflow"
    string completeTime; "date time for process completed and user notified"
    )

