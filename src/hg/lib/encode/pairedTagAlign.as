table pairedTagAlign
"Peaks format (BED 6+)"
(
    string chrom;      "Reference sequence chromosome or scaffold"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of item"
    uint   score;      "Score from 0-1000"
    char[1] strand;    "+ or -"
    string seq1;       "Sequence of first read"
    string seq2;       "Sequence of second read"
)
