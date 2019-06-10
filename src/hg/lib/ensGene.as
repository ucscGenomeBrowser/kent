table ensGene
"Ensembl gene predictions."
    (
    string name;	"Ensembl transcript ID"
    string chrom;	"Reference sequence chromosome or scaffold"
    char[1] strand;     "+ or - for strand"
    uint txStart;	"Transcription start position (or end position for minus strand item)"
    uint txEnd;         "Transcription end position (or start position for minus strand item)"
    uint cdsStart;	"Coding region start (or end position for minus strand item)"
    uint cdsEnd;        "Coding region end (or start position for minus strand item)"
    uint exonCount;     "Number of exons"
    uint[exonCount] exonStarts; "Exon start positions (or end positions for minus strand item)"
    uint[exonCount] exonEnds;   "Exon end positions (or start positions for minus strand item)"
    uint score;         "always 0 for Ensembl genes"
    string name2;       "Ensembl gene ID"
    string cdsStartStat; "Status of CDS start annotation (none, unknown, incomplete, or complete)" 
    string cdsEndStat;   "Status of CDS end annotation (none, unknown, incomplete, or complete)"
    int[exonCount] exonFrames; "Exon frame {0,1,2}, or -1 if no frame for exon"
    )
