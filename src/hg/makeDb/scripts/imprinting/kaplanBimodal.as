table kaplanBimodal
"Regions where DNA fragments carry two distinct methylation states (Rosenski et al. 2025)"
    (
    string  chrom;          "Reference sequence chromosome or scaffold"
    uint    chromStart;     "Start position in chromosome"
    uint    chromEnd;       "End position in chromosome"
    string  name;           "Number of cell types in which the region is bimodal"
    uint    score;          "Number of cell types sharing the region, scaled to 0-1000"
    char[1] strand;         "Not applicable, always ."
    uint    thickStart;     "Start of where display should be thick"
    uint    thickEnd;       "End of where display should be thick"
    uint    itemRgb;        "Colour, shaded light to dark by how many cell types share the region"
    lstring cellTypes;      "Cell types|cell types in which the region shows bimodal methylation"
    uint    cellTypeCount;  "Cell type count|number of cell types in which the region is bimodal"
    string  truncated;      "Note|set when the cell type list was cut off in the file published by the authors"
    string  liftNote;      "Lifting note|set when hg38 inserted sequence inside the region, so that its boundaries no longer match the published hg19 ones"
    )
