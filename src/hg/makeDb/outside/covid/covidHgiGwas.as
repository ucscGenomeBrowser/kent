table covidHgiGwas
"Meta-analysis from COVID 19 Host Genetics Initiative (covid19hg.org).  BED 9+12 for lollipop display"
    (
    string   chrom;            "Reference sequence chromosome or scaffold"
    uint     chromStart;       "Start position in chrom"
    uint     chromEnd;         "End position in chrom"
    string   name;             "dbSNP Reference SNP (rs) identifier or <chromNum>:<pos>"
    uint     score;            "Score from 0-1000, derived from p-value"
    char[1]  strand;           "Unused.  Always '.'"
    uint     thickStart;       "Start position in chrom"
    uint     thickEnd;         "End position in chrom"
    uint     color;            "Red (positive effect) or blue (negative) 
    double   effectSize;       "Effect size (beta coefficient)
    double   effectSizeSE;     "Effect size standard error"
    string   pValue;           "p-value"
    double   pValueLog;        "-log10 p-value"
    string   pValueHet;        "p-value from Cochran's Q heterogeneity test"
    lstring  ref;              "Non-effect allele"
    lstring  alt;              "Effect allele"
    double   alleleFreq;       "Allele frequency among the samples"
    uint     sampleN;          "Total sample size (sum of study sample sizes)"
    uint     sourceCount;      "Number of studies"
    uint     _radius;          "Lollipop radius; scaled ratio of sourceCount to total studies, for display"
    double   _effectSizeAbs;   "Effect size, abs value for display"
    )
