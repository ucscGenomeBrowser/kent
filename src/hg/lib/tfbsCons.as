table tfbsCons
"tfbsCons Data"
    (
    string chrom;      "Human chromosome"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of item"
    uint   score;      "Score from 0-1000"
    char[1] strand;    "+ or -"
    char[6] species;  "common name, scientific name"
    char[64] factor;  "factor "
    char[10] id;  "id"
    )
