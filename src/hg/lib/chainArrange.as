table chainArrange
"A list of assembly rearrangements"
    (
    string chrom;      "Chromosome (or contig, scaffold, etc.)"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of item"
    uint   score;      "Score from 0-1000"
    char[1] strand;    "+ or -"
    uint thickStart;   "Start of where display should be thick (start codon)"
    uint thickEnd;     "End of where display should be thick (stop codon)"
    uint itemRgb;      "Color"

    lstring source;     "List of assemblies where break occures"
    uint querySize;      "size of item on query side"
    string label;       "label for item"
    lstring _mouseover;       "mouseOver for item"
    )
