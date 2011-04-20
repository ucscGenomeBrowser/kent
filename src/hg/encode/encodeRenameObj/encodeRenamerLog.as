table encodeRenamerLog
"Log encode rename activity"
    (
    uint id;    "Auto-incrementing ID"
    string oldObj;      "Original Object Name"
    string newObj;      "New Object Name"
    string user;        "User who did it"
    string when;        "When did this rename happen - hack to datetime in .sql"
    string state;       "What state is it in? start, error, done"
    lstring errors;     "Log warn, errnoWarn, errAbort"
    lstring changes;    "Log changes such as file or table renames"
    lstring notes;      "Additional information"
    )
