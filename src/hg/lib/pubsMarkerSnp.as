table pubsMarkerSnp
"Text to Genome project marker bed: bed4 plus one additional field for count"
    (
    string chrom;         "chromosome"
    int chromStart;       "start position on chromosome"
    int chromEnd;         "end position on chromosome"
    string name;          "name of marker, e.g. gene like TP53 or rs12345"
    int matchCount;       "number of articles that contain matches for this marker"
    )
