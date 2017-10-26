table gtexEqtlCluster
"BED5+ of eQTLs (variants affecting gene expression) with a target (gene or tissue), and lists of values related to combined factors (e.g. tissues or genes)
    (
    string chrom;      "Reference sequence chromosome or scaffold"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of variant (rsID or GTEx identifier if none)"
    uint   score;      "Score from 0-1000"
    string targetId;   "Identifier of target (gene or tissue)
    string target;     "Name of target (gene or tissue)
    int distance;      "Distance from TSS"
    float maxEffect;   "Maximum absolute value effect size in cluster"
    char[1] effectType; "+, -, 0 (for mixed)"
    float maxPvalue;   "Maximum -log10 pValue in cluster"
    uint expCount;     "Number of experiment values"
    string[expCount] expNames; "Comma separated list of experiment names (e.g. tissue or gene)"
    float[expCount] expScores; "Comma separated list of effect size values"
    float[expCount] expPvals; "Comma separated list of -log10 transformed p-values"
    float[expCount] expProbs; "Comma separated list of probabilities variant is in high confidence causal set"
    )
