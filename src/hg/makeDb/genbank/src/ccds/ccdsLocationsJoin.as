table ccdsLocationsJoin
"parses join used to get exon locations"
    (
    int ccds_uid;         "CcdsUids.ccds_uid"
    int ccds_version;     "GroupVersions.ccds_version"
    char[2] chrom;        "Locations_GroupVersions.chromosome, maybe XY"
    char[1] strand;       "Groups.orientation"
    int start;            "Locations.chr_start"
    int stop;             "Locations.chr_stop"
    )
