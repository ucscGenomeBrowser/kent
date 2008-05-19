table transMapSrc
"shared about transMap source alignments"
    (
    char[16] db;        "source db"
    string id;          "id of source alignment"
    string chrom;       "chromosome"
    uint chromStart;    "chrom start location"
    uint chromEnd;      "chrom end location"
    char strand;        "strand"
    float ident;        "identity"
    float aligned;      "fraction of sequence aligned"
    )
