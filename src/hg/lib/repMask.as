table repeatMaskOut
"Repeat Masker out format"
    (
    uint score;         "Smith-Waterman score."
    float percDiv;	"Percentage base divergence."
    float percDel;      "Percentage deletions."
    float percInc;      "Percentage inserts."
    string qName;       "Name of query."
    uint qStart;        "Start query position."
    uint qEnd;          "End query position."
    string qLeft;         "Bases left in query."
    char[1] strand;     "Query strand (+ or C)"
    string rName;       "Repeat name"
    string rFamily;       "Repeat name"
    string rStart;        "Start position in repeat."
    uint rEnd;          "End position in repeat."
    string rLeft;         "Bases left in repeat."
    )
