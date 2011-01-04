table docIdSub
"keeps track of files submitted to ENCODE docId system"
    (
    int ix;            "auto-increment ID"
    int status;        "current status: for background process"
    string assembly;   "assembly"
    string submitDate; "submission data"
    string md5sum;     "md5sum of file"
    string valReport;  "validation report"
    string valVersion; "validator version"
    string metaData;   "metaData RA"
    string submitPath; "file path in DDF line"
    string submitter;  "submitter id"
    )
