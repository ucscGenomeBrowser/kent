table codeBlastScore
"Table storing the codes and positional info for blast runs."
    (
    string qName;       "Name of item"
    char[1] code;    "Code"
    string evalue;   "evalue"
    uint GI;          "GI Number"
    float PI;        "Percent Identity"
    uint length;     "Alignment length"
    uint gap;         "gap length"
    uint score;		"score from evalue"
    uint seqstart;    "Where alignment begins"
    uint seqend;      "Where alignment ends"
    char[255] species;    "Code"
    char[255] product;    "Code"
    char[255] name;    "Code"
    )





