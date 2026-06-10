table masterMindGame
"A solved game of Mastermind drawn as a lollipop track. Each row is a guess, the top row is the secret code, and the small pegs to the right are the black/white feedback key pegs."
    (
    string chrom;      "Reference sequence chromosome or scaffold"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Peg label"
    uint   score;      "Board row, used for the vertical position of the peg"
    char[1] strand;    "Unused"
    uint   thickStart; "Unused"
    uint   thickEnd;   "Unused"
    uint   reserved;   "Peg color as R,G,B"
    uint   size;       "Peg radius scale (large for code pegs, small for key pegs)"
    lstring _mouseOver; "Mouseover text"
    )
