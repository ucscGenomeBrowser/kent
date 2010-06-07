table trackVersion
"version information for database tables to monitor data loading history"
    (
    int ix;            "auto-increment ID"
    string db;         "UCSC database name"
    string name;       "table name in database"
    string who;        "Unix userID that performed this update"
    string version;    "version string, whatever is meaningful for data source"
    string updateTime; "YYYY-MM-DD HH:MM:SS most-recent-update time"
    string comment;    "other comments about version"
    string source;     "perhaps a URL for the data source"
    string dateReference;     "Ensembl date string for archive reference"
    )
