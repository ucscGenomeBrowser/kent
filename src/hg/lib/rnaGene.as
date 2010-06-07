table rnaGene
"Describes functional RNA genes."
    (
    string chrom;	"Reference sequence chromosome or scaffold"
    uint chromStart;    "Start position in chromosome"
    uint chromEnd;      "End position in chromosome"
    string name;        "Name of gene"
    uint score;         "Score from 0 to 1000"
    char[1] strand;     "Strand: + or -"
    string source;      "Source as in Sean Eddy's files."
    string type;        "Type - snRNA, rRNA, tRNA, etc."
    float fullScore;    "Score as in Sean Eddy's files."
    ubyte isPsuedo;      "TRUE(1) if psuedo, FALSE(0) otherwise"
    )
