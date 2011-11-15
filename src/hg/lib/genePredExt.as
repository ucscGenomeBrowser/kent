table genePredExt
"A gene prediction with some additional info."
    (
    string name;	"Name of gene (usually transcript_id from GTF)"
    string chrom;	"Reference sequence chromosome or scaffold"
    char[1] strand;     "+ or - for strand"
    uint txStart;	"Transcription start position"
    uint txEnd;         "Transcription end position"
    uint cdsStart;	"Coding region start"
    uint cdsEnd;        "Coding region end"
    uint exonCount;     "Number of exons"
    uint[exonCount] exonStarts; "Exon start positions"
    uint[exonCount] exonEnds;   "Exon end positions"
    uint score;         "score"
    string name2;       "Alternate name (e.g. gene_id from GTF)"
    string cdsStartStat; "enum('none','unk','incmpl','cmpl')"
    string cdsEndStat;   "enum('none','unk','incmpl','cmpl')"
    int[exonCount] exonFrames; "Exon frame {0,1,2}, or -1 if no frame for exon"
    )

