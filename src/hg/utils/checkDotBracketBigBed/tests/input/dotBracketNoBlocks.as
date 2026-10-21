table dotBracketNoBlocks
"bed6 plus an RNA secondary structure dot-bracket string"
    (
    string  chrom;       "Reference sequence chromosome or scaffold"
    uint    chromStart;  "Start position in chromosome"
    uint    chromEnd;    "End position in chromosome"
    string  name;        "Name of item"
    uint    score;       "Score from 0-1000"
    char[1] strand;      "+ or -"
    lstring dotBracket;  "Dot-bracket structure, transcript order, space where unpredicted"
    )
