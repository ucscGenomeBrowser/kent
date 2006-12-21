table agpFrag
"How to get through chromosome based on fragments"
    (
    string chrom;	"Reference sequence chromosome or scaffold"
    uint chromStart;	"start position in chromosome"
    uint chromEnd;	"end position in chromosome"
    int ix;             "ix of this fragment (useless)"
    char[1] type;       "(W)GS contig, (P)redraft, (D)raft, (F)inished or (O)ther"
    string frag;        "which fragment"
    uint fragStart;     "start position in frag"
    uint fragEnd;       "end position in frag"
    char[1] strand;     "+ or - (orientation of fragment)"
    )

