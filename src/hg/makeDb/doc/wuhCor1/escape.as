table whelanEscape
"Browser extensible data (8 fields) plus more"
    (
    string chrom;      "Chromosome (or contig, scaffold, etc.)"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of item"
    uint   score;      "Score from 0-1000"
    char[1] strand;    "+ or -"
    uint thickStart;   "Start of where display should be thick (start codon)"
    uint thickEnd;     "End of where display should be thick (stop codon)"
    uint itemRgb;     "color"
    string nucls;       "List of mutations (DNA)"
    string prots;       "List of mutations (as above, but expressed as amino acids)"
    string mabs;       "List of antibodies (one per mutation)"
    lstring _mouseOver;       "mouse over text"
    )
