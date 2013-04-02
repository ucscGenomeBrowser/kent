
table edwUser
"Someone who submits files to or otherwise interacts with big data warehouse"
    (
    char[64] sid;       "sha384 generated base64 encoded unique user ID"
    char[64] access;    "access code - hashed from password and stuff"
    string email;       "Email handle, the main identifier."
    )

table edwHost
"A web host we have collected files from - something like www.ncbi.nlm.gov or google.com"
    (
    uint id;            "Autoincremented host id"
    string name;        "Name (before DNS lookup)"
    bigint lastOkTime;   "Last time host was ok in seconds since 1970"
    bigint lastNotOkTime;  "Last time host was not ok in seconds since 1970"
    bigint firstAdded;     "Time host was first seen"
    lstring errorMessage; "If non-empty contains last error message from host. If empty host is ok"
    bigint uploadAttempts;  "Number of times things have been uploaded from this host"
    bigint historyBits; "Upload history with most recent in least significant bit. 0 for connection failed, 1 for success"
    )

table edwSubmitDir
"An external data directory we have collected a submit from"
    (
    uint id;            "Autoincremented id"
    lstring url;        "Web-mounted directory. Includes protocol, host, and final '/'"
    uint hostId;        "Id of host it's on"
    bigint lastOkTime;   "Last time submit dir was ok in seconds since 1970"
    bigint lastNotOkTime;  "Last time submit dir was not ok in seconds since 1970"
    bigint firstAdded;     "Time submit dir was first seen"
    lstring errorMessage; "If non-empty contains last error message from dir. If empty dir is ok"
    bigint uploadAttempts;  "Number of times uploads attempted fromt this submit directory"
    bigint historyBits; "Upload history with most recent in least significant bit. 0 for upload failed, 1 for success"
    )

table edwFile
"A file we are tracking that we intend to and maybe have uploaded"
    (
    uint id;                    "Autoincrementing file id"
    char[16] licensePlate;      "A abc123 looking license-platish thing"
    uint submitId;          "Links to id in submit table"
    lstring submitFileName;     "File name in submit relative to submit dir"
    lstring edwFileName;        "File name in big data warehouse relative to edw root dir"
    bigint startUploadTime;     "Time when upload started - 0 if not started"
    bigint endUploadTime;       "Time when upload finished - 0 if not finished"
    bigint updateTime;          "Update time (on system it was uploaded from)"
    bigint size;                "File size"
    char[32] md5;               "md5 sum of file contents"
    lstring tags;               "CGI encoded name=val pairs from manifest"
    lstring errorMessage; "If non-empty contains last error message from upload. If empty upload is ok"
    )

table edwSubmit
"A data submit, typically containing many files.  Always associated with a submit dir."
    (
    uint id;                 "Autoincremented submit id"
    lstring url;              "Url to validated.txt format file. We copy this file over and give it a fileId if we can." 
    bigint startUploadTime;   "Time at start of submit"
    bigint endUploadTime;     "Time at end of upload - 0 if not finished"
    char[64] userSid;        "Connects to user table sid field"
    uint submitFileId;       "Points to validated.txt file for submit."
    uint submitDirId;    "Points to the submitDir"
    uint fileCount;          "Number of files that will be in submit if it were complete."
    lstring errorMessage; "If non-empty contains last error message from submit. If empty submit is ok"
    )

table edwSubmitLog
"Log of status messages received during submit process"
    (
    uint id;    "Autoincremented id"
    uint submitId;  "Id in submit table"
    lstring message;    "Some message probably scraped out of stderr or something"
    )

table edwSubscribingProgram
"A program that wants to be called when a file arrives or a submit finishes"
    (
    uint id;             "ID of daemon"
    double runOrder;     "Determines order programs run in. In case of tie lowest id wins."
    string filePattern;  "A string with * and ? wildcards to match files we care about"
    string hubPattern;   "A string with * and ? wildcards to match hub URLs we care about"
    string tagPattern;   "A string of cgi encoded name=val pairs where vals have wildcards"
    string onFileStartUpload;       "A unix command string to run with a %u where file id goes"
    string onFileEndUpload;         "A unix command string to run with a %u where file id goes"
    string onSubmitStartUpload; "A unix command string to run with %u where submit id goes"
    string onSubmitEndUpload;   "A unix command string to run with %u where submit id goes"
    )
