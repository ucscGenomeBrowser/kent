table pscreen
"P-Screen (BDGP Gene Disruption Project) P el. insertion locations/genes"
    (
    string chrom;      "Reference sequence chromosome or scaffold"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of item (mutant strain with P el. insert here)"
    uint   score;      "Score from 0-1000 (placeholder! for bed 6 compat)"
    char[1] strand;    "+ or -"
    uint stockNumber;  "Mutant strain stock number, for ordering"
    uint geneCount;    "Number of genes disrupted by this insert"
    string[geneCount] geneIds;   "IDs of genes disrupted"
    )
