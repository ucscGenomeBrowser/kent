
table bdwUser
"Someone who submits files to or otherwise interacts with big data warehouse"
    (
    char[64] sid;       "sha384 generated base64 encoded unique user ID"
    char[64] access;    "access code - hashed from password and stuff"
    string email;       "Email handle, the main identifier."
    )

table bdwHost
"A web host we have collected files from - something like www.ncbi.nlm.gov or google.com"
    (
    uint id;            "Autoincremented host id"
    string name;        "Name (before DNS lookup)"
    bigint lastOkTime;   "Last time host was ok in seconds since 1970"
    bigint lastNotOkTime;  "Last time host was not ok in seconds since 1970"
    bigint firstAdded;     "Time host was first seen"
    lstring errorMessage; "If non-empty contains last error message from host. If empty host is ok"
    bigint uploadCount;  "Number of times things have been uploaded from this host"
    bigint historyBits; "Upload history with most recent in least significant bit. 0 for connection failed, 1 for success"
    )

table bdwSubmissionDir
"An external data directory we have collected a submission from"
    (
    uint id;            "Autoincremented id"
    lstring url;        "Web-mounted directory. Includes protocol, host, and final '/'"
    uint hostId;        "Id of host it's on"
    bigint lastOkTime;   "Last time submission dir was ok in seconds since 1970"
    bigint lastNotOkTime;  "Last time submission dir was not ok in seconds since 1970"
    bigint firstAdded;     "Time submission dir was first seen"
    lstring errorMessage; "If non-empty contains last error message from dir. If empty dir is ok"
    bigint uploadAttempts;  "Number of times uploads attempted fromt this submission directory"
    bigint historyBits; "Upload history with most recent in least significant bit. 0 for upload failed, 1 for success"
    )

table bdwFile
"A file we are tracking that we intend to and maybe have uploaded"
    (
    uint id;                    "Autoincrementing file id"
    char[16] licensePlate;      "A abc123 looking license-platish thing"
    uint submissionId;          "Links to id in submission table"
    lstring submitFileName;     "File name in submission relative to submission dir"
    lstring bdwFileName;        "File name in big data warehouse relative to bdw root dir"
    bigint startUploadTime;     "Time when upload started - 0 if not started"
    bigint endUploadTime;       "Time when upload finished - 0 if not finished"
    bigint updateTime;          "Update time (on system it was uploaded from)"
    bigint size;                "File size"
    char[32] md5;               "md5 sum of file contents"
    lstring tags;               "CGI encoded name=val pairs from manifest"
    lstring errorMessage; "If non-empty contains last error message from upload. If empty upload is ok"
    bigint uploadAttempts;  "Number of times file upload attempted"
    bigint historyBits; "Upload history with most recent in least significant bit. 0 for connection failed, 1 for success"
    )

table bdwSubmission
"A data submission, typically containing many files.  Always associated with a submission dir."
    (
    uint id;                 "Autoincremented submission id"
    lstring url;              "Url to validated.txt format file. We copy this file over and give it a fileId if we can." 
    bigint startUploadTime;   "Time at start of submission"
    bigint endUploadTime;     "Time at end of upload - 0 if not finished"
    char[64] userSid;        "Connects to user table sid field"
    uint submitFileId;       "Points to validated.txt file for submission."
    lstring errorMessage; "If non-empty contains last error message from submission. If empty submission is ok"
    )

table bdwSubmissionLog
"Log of status messages received during submission process"
    (
    uint id;    "Autoincremented id"
    uint submissionId;  "Id in submission table"
    lstring message;    "Some message probably scraped out of stderr or something"
    )

table bdwSubscribingProgram
"A program that wants to be called when a file arrives or a submission finishes"
    (
    uint id;             "ID of daemon"
    double runOrder;     "Determines order programs run in. In case of tie lowest id wins."
    string filePattern;  "A string with * and ? wildcards to match files we care about"
    string hubPattern;   "A string with * and ? wildcards to match hub URLs we care about"
    string tagPattern;   "A string of cgi encoded name=val pairs where vals have wildcards"
    string onFileStartUpload;       "A unix command string to run with a %u where file id goes"
    string onFileEndUpload;         "A unix command string to run with a %u where file id goes"
    string onSubmissionStartUpload; "A unix command string to run with %u where submission id goes"
    string onSubmissionEndUpload;   "A unix command string to run with %u where submission id goes"
    )
