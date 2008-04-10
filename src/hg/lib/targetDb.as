table targetDb
"Description of non-genomic target sequences (e.g. native mRNAs for PCR)"
    (
    string name;	"Identifier for this target"
    string description;	"Brief description for select box"
    string db;		"Database to which target has been mapped"
    string pslTable;    "PSL table in db that maps target coords to db coords"
    string seqTable;    "Table in db that has extFileTable indices of target sequences"
    string extFileTable; "Table in db that has .id, .path, and .size of target sequence files"
    string seqFile;	"Target sequence file path (typically /gbdb/$db/targetDb/$name.2bit)"
    float priority;     "Relative priority compared to other targets for same db (smaller numbers are higher priority)"
    timestamp time;	"Time at which this record was updated -- should be newer than db tables (so should blat server)"
    )
