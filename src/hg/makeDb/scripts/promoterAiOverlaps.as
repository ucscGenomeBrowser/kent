table promoterAiOverlaps
"PromoterAI overlap positions where transcripts disagree on score"
    (
    string chrom;        "Chromosome"
    uint chromStart;     "Start position (0-based)"
    uint chromEnd;       "End position"
    string name;         "Gene name(s) (comma-separated if multiple)"
    uint score;          "|PromoterAI| * 1000 (0-1000), based on largest-magnitude score"
    char[1] strand;      "Consensus transcript strand ('.' if transcripts disagree)"
    uint thickStart;     "Thick start"
    uint thickEnd;       "Thick end"
    uint reserved;       "Item RGB color (red=over-expression, blue=under-expression)"
    string alt;          "Alternate allele"
    lstring transcripts; "Ensembl transcript IDs (comma-separated)"
    lstring scores;      "PromoterAI scores per transcript (comma-separated)"
    lstring strands;     "Transcript strands (comma-separated, aligned with transcripts)"
    float scoreDiff;     "Maximum score difference across transcripts"
    string _mouseOver;   "Mouse over text"
    )
