table cosmicNew
"Catalog of Somatic Mutations in Cancer (COSMIC)"
    (
    string chrom;          "Reference sequence chromosome or scaffold"
    uint   chromStart;     "Start position in chromosome"
    uint   chromEnd;       "End position in chromosome"
    string name;           "COSMIC mutation ID"
    string geneName;       "Gene: HUGO symbol, mir-* ID, etc"
    string transcriptAcc;  "ENST, NM_, XM_ or other transcript accession"
    string mutDescription; "description of effect of variant on gene"
    string hgvsCds;        "HGVS variant nomenclature representation of mutation in CDS"
    string hgvsProt;       "HGVS variant nomenclature representation of mutation in protein"
    lstring ntChange;      "nucleotide sequence change"
    lstring aaChange;      "amino acid sequence change"
    string tumorSite;      "organ or tissue from which tumor sample was taken"
    int mutatedSamples;    "number of samples with the mutation"
    int examinedSamples;   "number of samples examined"
    )
