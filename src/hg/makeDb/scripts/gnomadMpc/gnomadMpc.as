table gnomadMpc
"gnomAD MPC score per variant, folded across transcripts"
    (
    string chrom;             "Chromosome"
    uint   chromStart;        "Start position (0-based)"
    uint   chromEnd;          "End position (exclusive)"
    string name;              "REF>ALT"
    uint   score;             "Max MPC scaled 0-1000"
    char[1] strand;           "Strand (always .)"
    uint   thickStart;        "Thick start"
    uint   thickEnd;          "Thick end"
    uint   reserved;          "itemRgb"
    string ref;               "Reference allele"
    string alt;               "Alternate allele"
    float  mpcMax;            "MPC|Max MPC score across transcripts"
    lstring transcripts;      "Transcripts|Ensembl transcript IDs (comma-separated)"
    lstring mpcByTranscript;  "Per-transcript MPC|MPC score per transcript (aligned with Transcripts)"
    uint   nTranscripts;      "Transcripts scoring|Number of transcripts scoring this variant"
    )
