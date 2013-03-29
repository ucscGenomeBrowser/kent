
table bdwUser
"Someone who submits files to or otherwise interacts with big data warehouse"
    (
    char[64] sid;        "sha512 generated unique user ID."
    string email;       "Email handle, the main identifier."
    )

table bdwHub
"An external data hub we have collected files from"
    (
    uint id;            "Autoincremented hub id"
    lstring url;        "Hub url - points to directory containing hub.txt file"
    string shortLabel;  "Hub short label from hub.txt file"
    string longLabel;   "Hub long label"
    ulong lastOkTime;   "Last time hub was ok in seconds since 1970"
    ulong lastNotOkTime;  "Last time hub was not ok in seconds since 1970"
    ulong firstAdded;     "Time hub was first seen"
    lstring errorMessage; "If non-empty contains last error message from hub. If empty hub is ok"
    )

table bdwHost
"An external host we have collected files from"
    (
    uint id;            "Autoincremented host id"
    string name;        "Name (before DNS lookup)"
    ulong lastOkTime;   "Last time hub was ok in seconds since 1970"
    ulong lastNotOkTime;  "Last time hub was not ok in seconds since 1970"
    ulong firstAdded;     "Time host was first seen"
    lstring errorMessage; "If non-empty contains last error message from host. If empty host is ok"
    )

table bdwSubmission
"A data submission, typically containing many files."
    (
    uint id;                 "Autoincremented submission id"
    ulong startUploadTime;   "Time at start of submission"
    ulong endUploadTime;     "Time at end of upload - 0 if not finished"
    char[64] userSid;        "Connects to user table sid field"
    uint hubId;              "Connect to hub table id field"
    lstring errorMessage; "If non-empty contains last error message from host. If empty host is ok"
    )

table bdwFile
"A file we are tracking that we intend to and maybe have uploaded"
    (
    uint id;                    "Autoincrementing host id"
    uint submission;            "Links to id in submission table"
    lstring hubFileName;        "File name in hub"
    char bdwName;               "A abc123 looking license-platish thing"
    lstring bdwFileName;        "File name in big data warehouse"
    ulong startUploadTime;      "Time when upload started - 0 if not started"
    ulong endUploadTime;        "Time when upload finished - 0 if not finished"
    ulong updateTime;           "Update time (on hub it was downloaded from)"
    ulong size;                 "File size"
    char[32] md5;               "md5 sum of file contents"
    lstring tags;               "CGI encoded name=val pairs from manifest"
    )

table bdwSubscriber
"A program that wants to be called when a file arrives or a submission finishes"
    (
    uint id;             "ID of subscriber"
    string onFileEndUpload; "A unix command string to run with a %u where file id goes"
    string onSubmissionEndUpload; "A unix command string to run with %u where submission id goes"
    )
    
