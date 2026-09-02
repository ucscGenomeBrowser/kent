table segdups
"HPRC segmental duplications (SEDEF / Eichler lab), assembly coordinates"
    (
    string chrom;      "Reference sequence chromosome or scaffold"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Unused (values are shown on mouseover)"
    uint   score;      "Fraction identity scaled 0-1000"
    char[1] strand;    "Orientation of the paralogous copy relative to this one"
    uint   thickStart; "Start of thick drawing"
    uint   thickEnd;   "End of thick drawing"
    uint   reserved;   "Item color (R,G,B)"
    string partner;    "Paralog partner region (seq:start-end)"
    float  pctMatch;   "Percent identity|Percent of matching bases in the alignment"
    uint   alnLen;     "Alignment length (bp)"
    uint   satBases;   "Satellite bases within the duplication"
    string uniqueId;   "SEDEF unique identifier for the duplication"
    string original;   "Whether this is an original (non-derived) duplication call"
    )
