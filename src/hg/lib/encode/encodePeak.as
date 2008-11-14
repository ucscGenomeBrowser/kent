table encodePeak
"Peaks format (BED 6+)"
(
    string chrom;        "Reference sequence chromosome or scaffold"
    uint   chromStart;   "Start position in chromosome"
    uint   chromEnd;     "End position in chromosome"
    string name;         "The name... probably just a period"
    uint   score;        "Score 1-1000"
    char[2] strand;      "+, -, or ."
    float  signalValue;  "Measurement of average enrichment for the region"
    float  pValue;       "Statistical significance of signal value (-log10)"
    float  qValue;       "Statistical significance normalized/corrected across datasets. Use -1 if no qValue is assigned."
    int    peak;         "Point-source called for this peak; 0-based offset from chromStart (use -1 if no point-source called)"
    uint blockCount;     "Number of blocks"
    uint[blockCount] blockSizes;   "Comma separated list of block sizes"
    uint[blockCount] blockStarts;  "Start positions relative to chromStart"    
)
