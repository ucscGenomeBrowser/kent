table gbProtAnn
"Protein Annotations from GenPept mat_peptide fields"
    (
    string chrom;      "Reference sequence chromosome or scaffold"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of item"
    string product;    "Protein product name"
    string note;       "Note (may be empty)"
    string proteinId; "GenBank protein accession(.version)"
    uint giId;        "GenBank db_xref number"
    )
