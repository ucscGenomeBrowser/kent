table pubsMarkerAnnot
"publications track: matches to markers (snp, gene, band) in text"
    (
    bigint articleId;     "internal article ID, created during download"
    int fileId;           "identifier of the file where the marker was found"
    int annotId;          "unique identifier of this marker within a file"
    string fileDesc;      "description of file where sequence was found"
    enum markerType;      "type of marker: snp, band or gene"
    string markerId;      "id of marker, e.g. TP53 or rs12354"
    enum section;         ""
    string snippet;       "flanking text around marker match"
    )
