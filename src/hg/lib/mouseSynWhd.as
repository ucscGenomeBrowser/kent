table mouseSynWhd
"Whitehead Synteny between mouse and human chromosomes (bed 6 +)."
    (
    string chrom;        "Reference sequence chromosome or scaffold"
    uint   chromStart;   "Start position in human chromosome"
    uint   chromEnd;     "End position in human chromosome"
    string name;         "Name of mouse chromosome"
    uint   score;        "Unused (bed 6 compatibility)"
    char[1] strand;      "+ or -"
    uint   mouseStart;   "Start position in mouse chromosome"
    uint   mouseEnd;     "End position in mouse chromosome"
    string segLabel;     "Whitehead segment label"
    )
