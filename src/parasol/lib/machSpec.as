table machSpec
"Basic info about a machine"
    (
    string name;     "Network name"
    int cpus;        "Number of CPUs we can use"
    int ramSize;     "Megabytes of memory"
    string scratchDir;  "Location of (local) scratch dir"
    int scratchSize; "Megabytes of local disk"
    string switchName; "Name of switch this is on"
    )
