table tadDomainEncode
"ENCODE contact-domain (TAD) interval with Arrowhead corner-detection scores"
    (
    string chrom;       "Chromosome"
    uint   chromStart;  "Domain start"
    uint   chromEnd;    "Domain end"
    string name;        "Biosample"
    float  cornerScore; "Arrowhead corner score: likelihood this is a contact-domain corner (higher = stronger)"
    float  uVarScore;   "Variance of the upper triangle of the domain contact submatrix"
    float  lVarScore;   "Variance of the lower triangle of the domain contact submatrix"
    float  upSign;      "Upper-triangle sign score: -1 x sum of signs of upper-triangle entries"
    float  loSign;      "Lower-triangle sign score: sum of signs of lower-triangle entries"
    )
