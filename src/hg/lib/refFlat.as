table refFlat
"A gene prediction with additional geneName field."
    (
    string  geneName;           "Name of gene as it appears in genome browser."
    string  name;               "Name of gene"
    string  chrom;              "Reference sequence chromosome or scaffold"
    char[1] strand;             "+ or - for strand"
    uint    txStart;            "Transcription start position"
    uint    txEnd;              "Transcription end position"
    uint    cdsStart;           "Coding region start"
    uint    cdsEnd;             "Coding region end"
    uint    exonCount;          "Number of exons"
    uint[exonCount] exonStarts; "Exon start positions"
    uint[exonCount] exonEnds;   "Exon end positions"
    )

