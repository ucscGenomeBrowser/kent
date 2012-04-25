table bedLogR
"BED9+1  Floating point score"
    (
    string chrom;      "Reference sequence chromosome or scaffold"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of item"
    uint   score;       "Score from 0-1000"
    char[1] strand;    "+ or - or . for unknown"
    uint thickStart;   "Start of where display should be thick (start codon)"
    uint thickEnd;     "End of where display should be thick (stop codon)"
    uint reserved;     "Color value R,G,B"
    float logR;        "logR value"
    )
