table ccdsLocationsJoin
"parses join used to get exon locations"
    (
    int ccds_uid;         "CcdsUids.ccds_uid"
    int lastest_version;  "CcdsUids.latest_version"
    char[2] chrom;        "Locations_GroupVersions.chromosome, maybe XY"
    char[1] strand;       "Groups.orientation"
    int start;            "Locations.chr_start"
    int stop;             "Locations.chr_stop"
    )
