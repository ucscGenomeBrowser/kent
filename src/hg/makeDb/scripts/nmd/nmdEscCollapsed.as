table nmdEscCollapsed
"NMD escape regions collapsed across overlapping transcripts"
    (
    string chrom;      "Chromosome (or contig, scaffold, etc.)"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Gene symbol"
    uint   score;      "Score from 0-1000"
    char[1] strand;    "+ or -"
    uint thickStart;   "Start of where display should be thick"
    uint thickEnd;     "End of where display should be thick"
    uint color;        "RGB color: red=rule 1, orange=rule 2, dark red=rule 3"
    string mouseover;  "Rule description and transcript count"
    lstring transcripts; "Comma-separated list of transcript IDs from which this region was derived"
    )
