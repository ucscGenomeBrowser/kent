table bed6
"Browser extensible data"
    (
    string chrom;      "Reference sequence chromosome or scaffold"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of item"
    uint   score;      "Score from 0-1000 (might not be applicable)"
    char[1] strand;    "+, - or . for unknown"
    uint thickStart;   "Start of where display should be thick"
    uint thickEnd;     "End of where display should be thick"
    lstring   transcriptId;      "Transcript IDs for the score"
    string   revelScore;      "Revel score for mutation on transcripts above"
    )
