table transMapInfo
"genomeDB information about transMap alignments"
    (
    string mappedId;       "id of mapped alignment"
    char[16] srcDb;        "source db"
    string srcId;          "id of source alignment"
    string mappingId;      "id of chain used for mapping"
    enum('unknown', 'all', 'syn', 'rbest') chainSubset; "chain subset used"
    )
