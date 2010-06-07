table jaxQTL3
"Quantitative Trait Loci from Jackson Lab / Mouse Genome Informatics"
    (
    string chrom;      "Reference sequence chromosome or scaffold"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of item"
    uint   score;      "Score from 0-1000 (bed6 compat.)"
    char[1] strand;    "+ or - (bed6 compat.)"
    uint   thickStart; "start of thick region"
    uint   thickEnd;   "start of thick region"
    string marker;     "MIT SSLP Marker w/highest correlation"
    string mgiID;      "MGI ID"
    string description;"MGI description"
    float  cMscore;    "cM position of marker associated with peak LOD score"
    string flank1;     "flanking marker 1"
    string flank2;     "flanking marker 2"
    )
