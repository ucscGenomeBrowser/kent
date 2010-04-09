table makeItems
"Browser extensible data"
    (
    uint bin;	       "Bin for range index"
    string chrom;      "Reference sequence chromosome or scaffold"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of item - up to 16 chars"
    char[1] strand;     "+ or - for strand"
    uint  score;	"0-1000.  Higher numbers are darker."
    string color;	"Comma separated list of RGB components.  IE 255,0,0 for red"
    lstring description; "Longer item description"
    )
