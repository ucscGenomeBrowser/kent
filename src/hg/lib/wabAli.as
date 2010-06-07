table wabAli
"Information on a waba alignment" 
    (
    string query;	"Foreign sequence name."
    uint qStart;        "Start in query (other species)"
    uint qEnd;          "End in query."
    char[1] strand;     "+ or - relative orientation."
    string chrom;       "Reference sequence chromosome or scaffold."
    uint chromStart;    "Start in chromosome."
    uint chromEnd;      "End in chromosome."
    uint milliScore;    "Base identity in parts per thousand."
    uint symCount;      "Number of symbols in alignment."
    lstring qSym;       "Query bases plus '-'s."
    lstring tSym;       "Target bases plus '-'s."
    lstring hSym;       "Hidden Markov symbols."
    )

