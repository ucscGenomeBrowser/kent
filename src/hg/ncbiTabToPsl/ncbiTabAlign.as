table ncbiTabAlign
"NCBI tab-seperated alignment file"
    (
    string target;      "target name"
    int alnId;          "arbitrary alignment id"
    string type;        "EXON = aligned region, GAP = query gap"
    int tStart;         "1-based start position in target"
    int tStop;          "end position in target"
    string query;       "query name"
    char[1] unused;     "unused"
    int qStart;         "1-based start position in query"
    int qStop;          "end position in query"
    float perId;        "percent identity"
    float score;        "score"
    )

