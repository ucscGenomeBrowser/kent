table ctgPos2
"Where a contig is inside of a chromosome including contig type information."
    (
    string contig;      "Name of contig"
    uint size;          "Size of contig"
    string chrom;       "Reference sequence chromosome or scaffold"
    uint chromStart;    "Start in chromosome"
    uint chromEnd;       "End in chromosome"
    char[1] type;       "(W)GS contig, (F)inished, (P)redraft, (D)raft, (O)ther"    )
