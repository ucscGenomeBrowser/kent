table sargassoSeaXra
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
    uint queryseqstart;    "Where alignment begins"
    uint queryseqend;      "Where alignment ends"
    char[255] species;    "Code"
    uint thisseqstart;    "Where alignment in sSea begins"
    uint thisseqend;    "Where alignment in sSea ends"
    )





