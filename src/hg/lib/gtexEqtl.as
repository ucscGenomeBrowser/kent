table gtexEqtl
"BED 9+ of eQTLs (variants affecting gene expression) with a target gene, effect size, p-value, and causal probability metric"
    (
    string chrom;      "Reference sequence chromosome or scaffold"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of variant (rsID or GTEx identifier if none)"
    uint   score;      "Score from 0-1000"
    char[1] strand;    "."
    uint thickStart;   "Start position"
    uint thickEnd;     "End position"
    uint reserved;     "R,G,B color: red for up-regulated, blue for down. Bright for high, grayed for low."
    string gene;       "Name of target gene"
    int distance;      "Distance from TSS"
    float effectSize;  "Effect size (FPKM). Regression slope calculated using quantile normalized expression"
    float pValue;      "Nominal p-value"
    float causalProb;  "Probability variant is in 95% credible set"
    )
