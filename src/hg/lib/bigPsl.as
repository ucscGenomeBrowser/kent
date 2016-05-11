table bigPsl
"bigPsl pairwise alignment"
    (
    string chrom;       "Reference sequence chromosome or scaffold"
    uint   chromStart;  "Start position in chromosome"
    uint   chromEnd;    "End position in chromosome"
    string name;        "Name or ID of item, ideally both human readable and unique"
    uint score;         "Score (0-1000)"
    char[1] strand;     "+ or - for strand"
    uint thickStart;    "Start of where display should be thick (start codon)"
    uint thickEnd;      "End of where display should be thick (stop codon)"
    uint reserved;       "RGB value (use R,G,B string in input file)"
    int blockCount;     "Number of blocks"
    int[blockCount] blockSizes; "Comma separated list of block sizes"
    int[blockCount] chromStarts; "Start positions relative to chromStart"

    uint    oChromStart;"Start position in other chromosome"
    uint    oChromEnd;  "End position in other chromosome"
    char[1] oStrand;    "+ or - for other strand"
    uint    oChromSize; "Size of other chromosome."
    int[blockCount] oChromStarts; "Start positions relative to oChromStart"

    lstring  oSequence;  "Sequence on other chrom (or edit list, or empty)"
    string   oCDS;       "CDS in NCBI format"

    uint    chromSize;"Size of target chromosome"

    uint match;        "Number of bases matched."
    uint misMatch; " Number of bases that don't match "
    uint repMatch; " Number of bases that match but are part of repeats "
    uint nCount;   " Number of 'N' bases "
    )

