table codeBlast
"Table storing the codes and positional info for blast runs."
    (
    short bin;         "bin for browser speed up"
    string chrom;      "Reference sequence chromosome or scaffold"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of item"
    uint score;        "Score (0-1000)"
    char[1] strand;    "Strand"
    string code;       "BLAST code scores"
    )
