table borf
"Parsed output from Victor Solovyev's bestorf program"
    (
    string name; "Name of mRNA"
    int size;   "Size of mRNA"
    char[1] strand; "+ or - or . in empty case"
    string feature; "Feature name - Always seems to be CDSo"
    int cdsStart;  "Start of cds (starting with 0)"
    int cdsEnd;    "End of cds (non-exclusive)"
    float score;   "A score of 50 or more is decent"
    int orfStart;  "Start of orf (not always same as CDS)"
    int orfEnd;  "Start of orf (not always same as CDS)"
    int cdsSize; "Size of cds in bases"
    char[3] frame; "Seems to be +1, +2, +3 or -1, -2, -3"
    lstring protein; "Predicted protein.  May be empty."
    )
