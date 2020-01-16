table mastermind
"mastermind bed: bed9 + various fields"
    (
    string chrom;      "Chromosome (or contig, scaffold, etc.)"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of item"
    uint   score;      "Score from 0-1000"
    char[1] strand;    "+ or -"
    uint thickStart;   "Start of where display should be thick (start codon)"
    uint thickEnd;     "End of where display should be thick (stop codon)"
    uint reserved;     "Used as itemRgb as of 2004-11-22"
    lstring url;     "Protein change and link to details"
    string gene;     "Gene symbol"
    string mmcnt1;     "MMCNT1 - articles with exact cDNA change"
    string mmcnt2;     "MMCNT2 - articles with exact cDNA and/or protein change"
    string mmcnt3;     "MMCNT3 - articles with same protein change"
    lstring _mouseOver;     "mouse over"
    )
