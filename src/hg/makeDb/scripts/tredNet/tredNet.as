table tredNet
"TREDNet candidate regulatory elements (enhancers or silencers)"
(
    string  chrom;        "Chromosome"
    uint    chromStart;   "Start position (0-based)"
    uint    chromEnd;     "End position"
    string  name;         "Sample count summary (e.g. '5 tissues')"
    uint    score;        "Score (0)"
    char[1] strand;       "Strand"
    uint    thickStart;   "thickStart (same as chromStart)"
    uint    thickEnd;     "thickEnd (same as chromEnd)"
    uint    reserved;     "Color (RGB)"
    uint    sampleCount;  "Number of samples where this element is predicted|Total count of cell types, tissues, or cell lines"
    lstring sampleList;   "Sample names|Comma-separated list of all cell types, tissues, or cell lines"
    lstring mouseOver;    "Mouse-over text|Full sample list (or count summary when list is long)"
    string  category;     "Dominant sample category|cell_type, tissue, cell_line, or mixed"
)
