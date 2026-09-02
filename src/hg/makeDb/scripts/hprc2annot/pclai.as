table pclai
"HPRC point-cloud local ancestry inference (pcLAI), assembly coordinates"
    (
    string chrom;      "Reference sequence chromosome or scaffold"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Unused (values are shown on mouseover)"
    uint   score;      "Confidence score (0-1000)"
    char[1] strand;    "+ or -"
    uint   thickStart; "Start of thick drawing"
    uint   thickEnd;   "End of thick drawing"
    uint   reserved;   "Item color (R,G,B)"
    string window;     "Local ancestry window id"
    string pca;        "Window PCA|PCA-space coordinates (PC1,PC2) of this window"
    string pcaSegment; "Segment PCA|PCA-space coordinates (PC1,PC2) of the containing ancestry segment"
    )
