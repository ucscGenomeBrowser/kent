table gtexGeneBed
"BED6+ with additional fields for gene and transcript IDs, and expression experiment scores
    (
    string chrom;       "Reference sequence chromosome or scaffold"
    uint   chromStart;  "Start position in chromosome"
    uint   chromEnd;    "End position in chromosome"
    string name;        "Gene symbol"
    uint   score;       "Score from 0-1000; derived from total median expression level across all tissues (log-transformed and scaled)"
    char[1] strand;     "+ or - for strand"
    string geneId;      "Ensembl gene ID, referenced in GTEx data tables"
    string geneType;    "GENCODE gene biotype"
    uint expCount;      "Number of tissues"
    float[expCount] expScores; "Comma separated list of per-tissue median expression levels (RPKM)"
    )
