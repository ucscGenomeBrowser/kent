table pubsSequenceAnnot
"publications track sequence data table"
    (
    bigint articleId;     "identifier of the article where the sequence was found"
    int fileId;           "identifier of the file where the sequence was found"
    bigint seqId;         "unique identifier of this sequence within a file"
    bigint annotId;       "articleId(10)+fileId(3)+seqId(5), refd by pubsSequenceAnnot"
    string fileDesc;      "description of file where sequence was found"
    lstring sequence;     "sequence"
    lstring snippet;      "flanking characters around sequence in article"
    string locations;     "comma-sep list of genomic locations where this sequence matches"
    )
