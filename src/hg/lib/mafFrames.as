table mafFrames
"codon frame assignment for MAF components"
    (
    string chrom;      "Chromosome"
    uint   chromStart; "Start range in chromosome"
    uint   chromEnd;   "End range in chromosome"
    string src;        "Name of sequence source in MAF"
    ubyte frame;       "frame (0,1,2) for first base(+) or last bast(-)"
    char[1] strand;    "+ or -"
    string name;       "Name of gene used to define frame"
    )
