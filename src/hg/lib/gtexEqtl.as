table gtexEqtl
"BED 9+ of expression Quantitative Trait Loci (eQTL). These are variants affecting gene expression"
    (
    string chrom;      "Reference sequence chromosome or scaffold"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Variant (rsID or GTEx identifier if none)"
    uint   score;      "Score from 0-1000"
    char[1] strand;    "."
    uint thickStart;   "Start position"
    uint thickEnd;     "End position"
    uint reserved;     "R,G,B color: red +effect, blue -effect. Bright for high, pale for lower (cutoff effectSize 2.0 RPKM)."
    string gene;       "Target gene"
    int distance;      "Distance from TSS"
    float effectSize;  "Effect size (FPKM)"
    float pValue;      "Nominal p-value"
    float causalProb;  "Probability variant is in 95% credible set"
    )
