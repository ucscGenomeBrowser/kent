table pclai
"HPRC point cloud local ancestry inference (pcLAI), assembly coordinates"
    (
    string chrom;      "Reference sequence chromosome or scaffold"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Unused (values are shown on mouseover)"
    uint   score;      "Confidence score (0-1000, higher is more confident)"
    char[1] strand;    "+ or -"
    uint   thickStart; "Start of thick drawing"
    uint   thickEnd;   "End of thick drawing"
    uint   reserved;   "Item color (R,G,B)"
    string window;     "Local ancestry window id (~1000 SNPs)"
    string pca;        "Window PCA|PCA-space coordinates (PC1,PC2) predicted for this window"
    string centroid;   "Ancestry centroid|Discretized pcLAI ancestry of this window, given as the (PC1,PC2) centroid of that ancestry cluster"
    )
