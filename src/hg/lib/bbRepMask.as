table bbRepMask
"BB Repeat Masker Representation"
    (
    ubyte seqId;	"Id of chromosome"
    uint seqStart;	"Start in sequence"
    uint seqEnd;        "End in sequence"
    ushort bin;		"Which bin it's in."
    string name;        "Name of repeat"
    uint score;         "Smith-Waterman score."
    char[1] strand;     "Query strand (+ or C)"
    string repFamily;       "Repeat name"
    int repStart;         "Start position in repeat."
    int repEnd;          "End position in repeat."
    int repLeft;         "Bases left in repeat."
    uint milliDiv;      "Base mismatches in parts per thousand"
    uint milliDel;      "Bases deleted in parts per thousand"
    uint milliIns;      "Bases inserted in parts per thousand"
    )
