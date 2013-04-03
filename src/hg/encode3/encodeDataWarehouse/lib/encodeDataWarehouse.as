
table edwUser
"Someone who submits files to or otherwise interacts with big data warehouse"
    (
    uint id;            "Autoincremented user ID"
    string name;        "user name"
    char[64] sid;       "sha384 generated user ID - used to identify user in secure way if need be"
    char[64] access;    "access code - sha385'd from password and stuff"
    string email;       "Email address - required"
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
    bigint openSuccesses;  "Number of times files have been opened ok from this host"
    bigint openFails;      "Number of times files have failed to open from this host"
    bigint historyBits; "Open history with most recent in least significant bit. 0 for connection failed, 1 for success"
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
    bigint openSuccesses;  "Number of times files have been opened ok from this dir"
    bigint openFails;      "Number of times files have failed to open from this dir"
    bigint historyBits; "Open history with most recent in least significant bit. 0 for upload failed, 1 for success"
    )

table edwFile
"A file we are tracking that we intend to and maybe have uploaded"
    (
    uint id;                    "Autoincrementing file id"
    char[16] licensePlate;      "A abc123 looking license-platish thing"
    uint submitId;              "Links to id in submit table"
    uint submitDirId;           "Links to id in submitDir table"
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
    uint userId;        "Connects to user table id field"
    uint submitFileId;       "Points to validated.txt file for submit."
    uint submitDirId;    "Points to the submitDir"
    uint fileCount;          "Number of files that will be in submit if it were complete."
    uint oldFiles;           "Number of files in submission that were already in warehouse."
    uint newFiles;           "Number of files in submission that are newly uploaded."
    lstring errorMessage; "If non-empty contains last error message. If empty submit is ok"
    )

table edwSubscriber
"Subscribers can have programs that are called at various points during data submission"
    (
    uint id;             "ID of subscriber"
    string name;         "Name of subscriber"
    double runOrder;     "Determines order subscribers run in. In case of tie lowest id wins."
    string filePattern;  "A string with * and ? wildcards to match files we care about"
    string dirPattern;   "A string with * and ? wildcards to match hub dir URLs we care about"
    string onFileStartUpload;       "A unix command string to run with a %u where file id goes"
    string onFileEndUpload;         "A unix command string to run with a %u where file id goes"
    string onSubmitStartUpload; "A unix command string to run with %u where submit id goes"
    string onSubmitEndUpload;   "A unix command string to run with %u where submit id goes"
    )
