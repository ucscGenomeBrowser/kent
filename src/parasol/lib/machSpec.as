table machSpec
"Basic info about a machine"
    (
    string name;     "Network name"
    int cpus;        "Number of CPUs we can use"
    int ramSize;     "Megabytes of memory"
    string tempDir;  "Location of (local) temp dir"
    string localDir;   "Location of local data dir"
    int localSize;     "Megabytes of local disk"
    string switchName; "Name of switch this is on"
    )
