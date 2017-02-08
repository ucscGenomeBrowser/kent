table refFlat
"A gene prediction with additional geneName field."
    (
    string  geneName;           "Name of gene as it appears in genome browser."
    string  name;               "Name of gene"
    string  chrom;              "Reference sequence chromosome or scaffold"
    char[1] strand;             "+ or - for strand"
    uint    txStart;            "Transcription start position (or end position for minus strand item)"
    uint    txEnd;              "Transcription end position (or start position for minus strand item)"
    uint    cdsStart;           "Coding region start (or end position for minus strand item)"
    uint    cdsEnd;             "Coding region end (or start position for minus strand item)"
    uint    exonCount;          "Number of exons"
    uint[exonCount] exonStarts; "Exon start positions (or end positions for minus strand item)"
    uint[exonCount] exonEnds;   "Exon end positions (or start positions for minus strand item)"
    )

