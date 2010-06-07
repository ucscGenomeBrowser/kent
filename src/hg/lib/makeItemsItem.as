table makeItemsItem
"An item in a makeItems type track."
    (
    uint bin;	       "Bin for range index"
    string chrom;      "Reference sequence chromosome or scaffold"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of item - up to 16 chars"
    uint  score;	"0-1000.  Higher numbers are darker."
    char[1] strand;     "+ or - for strand"
    uint   thickStart; "Start of thick part"
    uint   thickEnd;   "End position of thick part"
    uint itemRgb;	"RGB 8 bits each as in bed"
    lstring description; "Longer item description"
    uint id;		"Unique ID for item"
    )
