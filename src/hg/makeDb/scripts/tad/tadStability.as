table tadStability
"TAD boundary recurrence across 37 cell-type maps (McArthur & Capra 2021)"
    (
    string chrom;       "Chromosome"
    uint   chromStart;  "100 kb boundary bin start"
    uint   chromEnd;    "100 kb boundary bin end"
    string name;        "Boundary bin"
    uint   score;       "Rendering proxy for contexts (=round(contexts/37*1000)); the real datum is contexts"
    uint   contexts;    "Number of 37 cell-type maps with a boundary here (1-37)"
    float  percentile;  "Stability percentile (rank of contexts; from source)"
    )
