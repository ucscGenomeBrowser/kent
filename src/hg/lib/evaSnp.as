table evaSnp
"Variant data from EVA VCF files"
    (
    string  chrom;      "Reference sequence chromosome or scaffold"
    uint    chromStart; "Start position in chrom"
    uint    chromEnd;   "End position in chrom"
    string  name;       "Reference SNP identifier"
    uint    score;      "Not used"
    char[1] strand;     "Which DNA strand contains the observed alleles"
    string  ref;        "The sequence of the reference allele"
    string  alt;        "The sequences of the alternate alleles"
    string  class;      "The class of variant"
    string  submitters; "Source of the data"
    string  valid;      "The rs_validation status of the SNP"
    )
