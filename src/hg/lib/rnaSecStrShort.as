table rnaSecStrShort
"Browser extensible data with secondary structure string"
    (
    string  chrom;      "Reference sequence chromosome or scaffold"
    uint    chromStart; "Start position in chromosome"
    uint    chromEnd;   "End position in chromosome"
    string  name;       "Name of item"
    uint    score;      "Score from 0-1000"
    char[1] strand;     "+ or -"
    uint    size;       "Size of element."
    lstring secStr;     "Parentheses and '.'s which define the secondary structure"
    )
