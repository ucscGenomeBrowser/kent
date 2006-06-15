table mafFrames
"codon frame assignment for MAF components"
    (
    string chrom;      "Reference sequence chromosome or scaffold"
    uint   chromStart; "Start range in chromosome"
    uint   chromEnd;   "End range in chromosome"
    string src;        "Name of sequence source in MAF"
    ubyte frame;       "frame (0,1,2) for first base(+) or last bast(-)"
    char[1] strand;    "+ or -"
    string name;       "Name of gene used to define frame"
    int    prevEnd;    "chromEnd of previous part of gene, or -1 if none"
    int    nextStart;  "chromStart of next part of gene, or -1 if none"
    )
