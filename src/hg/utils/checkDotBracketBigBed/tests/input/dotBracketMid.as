table dotBracketMid
"bed12 with the dot-bracket string in the middle of three extra columns"
    (
    string  chrom;       "Reference sequence chromosome or scaffold"
    uint    chromStart;  "Start position in chromosome"
    uint    chromEnd;    "End position in chromosome"
    string  name;        "Name of item"
    uint    score;       "Score from 0-1000"
    char[1] strand;      "+ or -"
    uint    thickStart;  "Start of where display should be thick"
    uint    thickEnd;    "End of where display should be thick"
    uint    reserved;    "Used as itemRgb"
    int     blockCount;  "Number of blocks"
    int[blockCount] blockSizes;  "Comma separated list of block sizes"
    int[blockCount] chromStarts; "Start positions relative to chromStart"
    string  before;      "A column ahead of the dot-bracket one"
    lstring dotBracket;  "Dot-bracket structure, transcript order"
    string  after;       "A column behind the dot-bracket one"
    )
