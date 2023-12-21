table dosageSensitivityTriplo
"Triplosensitivity probability scores from Colins et al 2022"
    (
    string   chrom;      "Reference sequence chromosome or scaffold"
    uint     chromStart; "Start position in chrom"
    uint     chromEnd;   "End position in chrom"
    string   name;       "Reference SNP identifier"
    uint     score;      "Not used"
    char[1]  strand;     "Which DNA strand contains the observed alleles"
    uint     thickStart; "Same as chromStart"
    uint     thickEnd;   "Same as chromEnd"
    uint     itemRgb;    "RGB corresponding to evidence classification"
    string  ensGene;        "Ensembl gene ID"
    string  pTriplo;        "Probability score for triplosensitivity"
    )
