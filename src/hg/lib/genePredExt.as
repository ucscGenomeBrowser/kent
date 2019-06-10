table genePredExt
"A gene prediction with some additional info."
    (
    string name;	"Name of gene (usually transcript_id from GTF)"
    string chrom;	"Reference sequence chromosome or scaffold"
    char[1] strand;     "+ or - for strand"
    uint txStart;	"Transcription start position (or end position for minus strand item)"
    uint txEnd;         "Transcription end position (or start position for minus strand item)"
    uint cdsStart;	"Coding region start (or end position for minus strand item)"
    uint cdsEnd;        "Coding region end (or start position for minus strand item)"
    uint exonCount;     "Number of exons"
    uint[exonCount] exonStarts; "Exon start positions (or end positions for minus strand item)"
    uint[exonCount] exonEnds;   "Exon end positions (or start positions for minus strand item)"
    uint score;         "score"
    string name2;       "Alternate name (e.g. gene_id from GTF)"
    string cdsStartStat; "Status of CDS start annotation (none, unknown, incomplete, or complete)" 
    string cdsEndStat;   "Status of CDS end annotation (none, unknown, incomplete, or complete)"
    int[exonCount] exonFrames; "Exon frame {0,1,2}, or -1 if no frame for exon"
    )

