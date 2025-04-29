table evaSnp
"Variant data from EVA VCF files"
    (
    string   chrom;      "Reference sequence chromosome or scaffold"
    uint     chromStart; "Start position in chrom"
    uint     chromEnd;   "End position in chrom"
    string   name;       "Reference SNP identifier"
    uint     score;      "Not used"
    char[1]  strand;     "Which DNA strand contains the observed alleles"
    uint     thickStart; "Same as chromStart"
    uint     thickEnd;   "Same as chromEnd"
    uint     itemRgb;    "RGB value for color of item"
    lstring  ref;        "The sequence of the reference allele"
    lstring  alt;        "The sequences of the alternate alleles"
    string   varClass;   "The variant class (VC) from EVA Sequence Ontology term"
    lstring  submitters; "Submitter ID (SID) reporting a variant"
    lstring   ucscClass;  "Functional class per UCSC Variant Annotation Integrator"
    lstring   aaChange;  "Change in amino acid"
    )
