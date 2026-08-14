table nmdEscManeCollapsed
"NMD escape regions for MANE Select Plus Clinical (one row per transcript)"
    (
    string chrom;      "Chromosome (or contig, scaffold, etc.)"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Gene Symbol"
    uint   score;      "Score from 0-1000"
    char[1] strand;    "+ or -"
    uint thickStart;   "Start of where display should be thick"
    uint thickEnd;     "End of where display should be thick"
    uint color;        "RGB color: red=rule 1, orange=rule 2, dark red=rule 3, gold=rule 4"
    string mouseover;  "Rule description and transcript count"
    lstring transcripts; "Gencode Accession (ENST)"
    lstring ncbiIds;   "RefSeq Accession (NM_/NR_)"
    )
