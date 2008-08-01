table encodePeak
"Peaks format (BED 3+)"
(
    string chrom;        "Reference sequence chromosome or scaffold"
    uint   chromStart;   "Start position in chromosome"
    uint   chromEnd;     "End position in chromosome"
    float  signalValue;  "Measurement of average enrichment for the region"
    float  pValue;       "Statistical significance of signal value (-log10)"
    int    peak;         "Point-source called for this peak; 0-based offset from chromStart (use -1 if no point-source called)"
)
