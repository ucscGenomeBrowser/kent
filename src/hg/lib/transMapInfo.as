table transMapInfo
"information about transMap gene mappings"
    (
    char[12] srcDb;        "source db"
    char[8] chains;        "type of chains used for mapping"
    char[22] srcId;        "id of original gene"
    char[32] srcChrom;     "source chromosome"
    uint srcStart;         "source start location"
    uint srcEnd;           "source end location"
    uint srcExonCnt;       "number of exons in src gene"
    uint srcCdsExonCnt;    "number of CDS exons in src gene"
    uint srcBaseCnt;       "number of bases in src gene"
    uint srcCdsBaseCnt;    "number of CDS bases in src gene"
    char[22] mappedId;     "id of mapped gene"
    char[32] mappedChrom;  "mapped chromosome"
    uint mappedStart;      "mapped start location"
    uint mappedEnd;        "mapped end location"
    uint mappedExonCnt;    "number of exons mapped"
    uint mappedCdsExonCnt; "number of CDS exons mapped"
    uint mappedBaseCnt;    "number of bases mapped"
    uint mappedCdsBaseCnt; "number of CDS bases mapped"
    )

