table hapmapSnps
"HapMap allele counts by population and derived alleles"
    (
    string  chrom;      "Chromosome"
    uint    chromStart; "Start position in chrom (0 based)"
    uint    chromEnd;   "End position in chrom (1 based)"
    string  name;       "Reference SNP identifier from dbSnp"
    uint    score;      "Not used"
    char[1] strand;     "Which genomic strand contains the observed alleles"

    char[1] hReference; "Reference allele for human"
    char[1] hOther;     "Other allele for human"

    char[1] cState;     "Chimp state"
    char[1] rState;     "Rhesus state"
    uint    cQual;      "chimp quality score"
    uint    rQual;      "rhesus quality score"

    uint    rYri;       "Reference allele count for the YRI population"
    uint    rCeu;       "Reference allele count for the CEU population"
    uint    rChb;       "Reference allele count for the CHB population"
    uint    rJpt;       "Reference allele count for the JPT population"
    uint    rJptChb;    "Reference allele count for the JPT+CHB population"

    uint    oYri;       "Other allele count for the YRI population"
    uint    oCeu;       "Other allele count for the CEU population"
    uint    oChb;       "Other allele count for the CHB population"
    uint    oJpt;       "Other allele count for the JPT population"
    uint    oJptChb;    "Other allele count for the JPT+CHB population"

    uint    nYri;       "Sample Size for the YRI population"
    uint    nCeu;       "Sample Size for the CEU population"
    uint    nChb;       "Sample Size for the CHB population"
    uint    nJpt;       "Sample Size for the JPT population"
    uint    nJptChb;    "Sample Size for the JPT+CHB population"
    )
