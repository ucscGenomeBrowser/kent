table mgcStage1
"stack of clone data"
    (
    int id_clone;       "clone id"
    int id_est;         "dbEST id"
    string p_end;       "5' or 3'"
    int id_vendor;      "id of clone arrayer"
    int id_cluster;     "unanchored cluster id within FLC"
    int priority;       "priority score stage: <256 = unknown, >256 = gi"
    byte live;          "is this current record 1=no, 2=yes"
    int version;        "version of clustering"
    string cdate;       "date of this version"
    byte picked;        "picked for stage 1: 1=no; 2 = yes"
    string pdate;       "date clone was picked"
    byte suppress;      "should this be suppressed: 1=no; 2 = yes"
    string organism;    "organism name"
    byte currpick;      "is a current pick? 1=no; 2=yes"
    )
