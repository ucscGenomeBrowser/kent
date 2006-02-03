table encodeStanfordRtPcr
"Stanford RTPCR in ENCODE Regions (BED5+)"
    (
    string chrom;      "Reference sequence chromosome or scaffold"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Accession of gene (or unknown/negative)"
    uint   score;      "Score from 0-1000"
    string primerPair; "Long name of primer pair"
    uint   count;      "Endogenous copies in cells"
    )
