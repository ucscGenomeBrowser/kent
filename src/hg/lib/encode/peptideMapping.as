table peptideMapping
"Format for genomic mappings of mass spec proteogenomic hits"
    (
    string chrom;      "Reference sequence chromosome or scaffold"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Peptide sequence of the hit"
    uint   score;      "Log e-value scaled to a score of 0 (worst) to 1000 (best)
    char[1] strand;    "+ or -"
    float  rawScore;   "Raw score for this hit, as estimated through HMM analysis"
    string spectrumId; 	"Non-unique identifier for the spectrum file"
    uint   peptideRank; "Rank of this hit, for peptides with multiple genomic hits"
    uint   peptideRepeatCount;    "Indicates how many times this same hit was observed"
    )
