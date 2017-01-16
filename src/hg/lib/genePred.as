table genePred
"A gene prediction."
    (
    string name;	"Name of gene"
    string chrom;	"Reference sequence chromosome or scaffold"
    char[1] strand;     "+ or - for strand"
    uint txStart;	"Transcription start position (or transcription end position if item is on minus strand)"
    uint txEnd;         "Transcription end position (or transcription start position if item is on minus strand)"
    uint cdsStart;	"Coding region start (or coding end position if item is on minus strand)"
    uint cdsEnd;        "Coding region end (or coding start position if item is on minus strand)"
    uint exonCount;     "Number of exons"
    uint[exonCount] exonStarts; "Exon start positions (or exon end positions if item is on minus strand)"
    uint[exonCount] exonEnds;   "Exon end positions (or exon start positions if item is on minus strand)"
    )
