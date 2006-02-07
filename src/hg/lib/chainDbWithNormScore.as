table chainWithNormScore
"Summary info about a chain of alignments, with normalized score"
    (
    double score; "score of chain"
    string tName; "Target sequence name"
    uint tSize; "Target sequence size"
    uint tStart; "Alignment start position in target"
    uint tEnd; "Alignment end position in target"
    string qName; "Query sequence name"
    uint qSize; "Query sequence size"
    char qStrand; "Query strand"
    uint qStart; "Alignment start position in query"
    uint qEnd; "Alignment end position in query"
    uint id; "chain id"
    double normScore; "normalized score = score / (bases matched in query)"
    )
