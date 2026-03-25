table promoterAiOverlaps
"PromoterAI overlap positions where transcripts disagree on score"
    (
    string chrom;        "Chromosome"
    uint chromStart;     "Start position (0-based)"
    uint chromEnd;       "End position"
    string name;         "Gene name"
    uint score;          "Score scaled 0-1000"
    char[1] strand;      "Strand"
    uint thickStart;     "Thick start"
    uint thickEnd;       "Thick end"
    uint reserved;       "Item RGB color"
    string alt;          "Alternate allele"
    lstring transcripts; "Ensembl transcript IDs (comma-separated)"
    lstring scores;      "PromoterAI scores (comma-separated)"
    float scoreDiff;     "Maximum score difference across transcripts"
    string _mouseOver;   "Mouse over text"
    )
