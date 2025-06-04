table decoration
"Browser extensible data (12 fields) plus information about what item this decorates and how."
    (
    string chrom;      "Chromosome (or contig, scaffold, etc.)"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of item"
    uint   score;      "Score from 0-1000"
    char[1] strand;    "+ or -"
    uint thickStart;   "Start of where display should be thick (start codon)"
    uint thickEnd;     "End of where display should be thick (stop codon)"
    uint color;        "Primary RGB color for the decoration"
    int blockCount;    "Number of blocks"
    int[blockCount] blockSizes; "Comma separated list of block sizes"
    int[blockCount] chromStarts; "Start positions relative to chromStart"
    string decoratedItem; "Identity of the decorated item in chr:start-end:item_name format"
    string style;      "Draw style for the decoration (e.g. block, glyph)"
    string fillColor;  "Secondary color to use for filling decoration, blocks, supports RGBA"
    string glyph;  "The glyph to draw in glyph mode; ignored for other styles"
    # repeat the parent mouseover, as the decorator is often wider than the parent feature
    lstring mouseOver;  "mouseOver text"
    )
