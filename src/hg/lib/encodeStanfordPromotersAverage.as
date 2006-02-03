table encodeStanfordPromotersAverage
"Stanford Promoter Activity in ENCODE Regions, average over all cell types (bed 9+)"
    (
    string chrom;      "Reference sequence chromosome or scaffold"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "accession of mRNA used to predict the promoter"
    uint   score;      "Score from 0-1000"
    char[1] strand;    "+ or -"
    uint thickStart;   "Placeholder for BED9 format -- same as chromStart"
    uint thickEnd;     "Placeholder for BED9 format -- same as chromEnd"
    uint reserved;     "Used as itemRgb"
    string geneModel;  "Gene model ID; same ID may have multiple predicted promoters"
    string description; "Gene description"
    float normLog2Ratio; "Normalized and log2 transformed Luciferase Renilla Ratio"
    )
