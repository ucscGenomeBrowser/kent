table tadDomainEncode
"ENCODE contact-domain (TAD) interval with Arrowhead corner-detection scores"
    (
    string chrom;       "Chromosome"
    uint   chromStart;  "Domain start"
    uint   chromEnd;    "Domain end"
    string name;        "Biosample"
    float  cornerScore; "Arrowhead corner score: likelihood this is a contact-domain corner (higher = stronger; can exceed 1)"
    float  uVarScore;   "Variance of Arrowhead-transformed values in the upper triangle"
    float  lVarScore;   "Variance of Arrowhead-transformed values in the lower triangle"
    float  upSign;      "Upper-triangle sign score: minus the mean sign of Arrowhead-transformed values (-1 to 1)"
    float  loSign;      "Lower-triangle sign score: mean sign of Arrowhead-transformed values (-1 to 1)"
    )
