table ensGene
"Ensembl gene predictions."
    (
    string name;	"Ensembl transcript ID"
    string chrom;	"Reference sequence chromosome or scaffold"
    char[1] strand;     "+ or - for strand"
    uint txStart;	"Transcription start position (or transcription end position if item is on minus strand)"
    uint txEnd;         "Transcription end position (or transcription start position if item is on minus strand)"
    uint cdsStart;	"Coding region start (or coding end position if item is on minus strand)"
    uint cdsEnd;        "Coding region end (or coding start position if item is on minus strand)"
    uint exonCount;     "Number of exons"
    uint[exonCount] exonStarts; "Exon start positions (or exon end positions if item is on minus strand)"
    uint[exonCount] exonEnds;   "Exon end positions (or exon start positions if item is on minus strand)"
    uint score;         "always 0 for Ensembl genes"
    string name2;       "Ensembl gene ID"
    string cdsStartStat; "enum('none','unk','incmpl','cmpl')"
    string cdsEndStat;   "enum('none','unk','incmpl','cmpl')"
    int[exonCount] exonFrames; "Exon frame {0,1,2}, or -1 if no frame for exon"
    )
