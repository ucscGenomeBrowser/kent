table gtexGeneBed
"BED6+ with additional fields for gene and transcript IDs, and expression experiment scores
    (
    string chrom;       "Reference sequence chromosome or scaffold"
    uint   chromStart;  "Start position in chromosome"
    uint   chromEnd;    "End position in chromosome"
    string name;        "Gene symbol"
    uint   score;      "Score from 0-1000"
    char[1] strand;     "+ or - for strand"
    string geneId;      "Ensembl gene ID, referenced in GTEx data tables"
    string transcriptId;       "Ensembl ID of Canonical transcript; determines genomic position"
    string transcriptClass;    "GENCODE transcript class (coding, nonCoding, pseudo)
    uint expCount;             "Number of experiment values"
    float[expCount] expScores; "Comma separated list of experiment scores"
    )
