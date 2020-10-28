table asmEquivalent
"Equivalence relationship of assembly versions, Ensembl: UCSC, NCBI genbank/refseq"
    (
    string source;          "assembly name"
    string destination;     "equivalent assembly name"
    enum ("ensembl", "ucsc", "genbank", "refseq") sourceAuthority; "origin of source assembly"
    enum ("ensembl", "ucsc", "genbank", "refseq") destinationAuthority; "origin of equivalent assembly"
    bigint   matchCount;       "number of exactly matching sequences"
    bigint   sourceCount;      "number of sequences in source assembly"
    bigint   destinationCount; "number of sequences in equivalent assembly"
    )
