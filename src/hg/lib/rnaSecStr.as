table rnaSecStr
"Browser extensible data"
    (
    string  chrom;      "Chromosome or FPC contig"
    uint    chromStart; "Start position in chromosome"
    uint    chromEnd;   "End position in chromosome"
    string  name;       "Name of item"
    uint    score;      "Score from 0-1000"
    char[1] strand;     "+ or -"
    uint    size;       "Size of element."
    lstring secStr;     "Parenthesis and '.'s which define the secondary structure"
    double[size]  conf; "Confidence of secondary-structure annotation per position (0.0-1.0)."
    )
