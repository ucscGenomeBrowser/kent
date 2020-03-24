table asmEquivalent
"Equivalence relationship of assembly versions, Ensembl: UCSC, NCBI genbank/refseq"
    (
    string source;          "assembly name"
    string destination;     "equivalent assembly name"
    enum ("ensembl", "ucsc", "genbank", "refseq") sourceAuthority; "origin of source assembly"
    enum ("ensembl", "ucsc", "genbank", "refseq") destinationAuthority; "origin of equivalent assembly"
    uint   matchCount;       "number of exactly matching sequences"
    uint   sourceCount;      "number of sequences in source assembly"
    uint   destinationCount; "number of sequences in equivalent assembly"
    )
