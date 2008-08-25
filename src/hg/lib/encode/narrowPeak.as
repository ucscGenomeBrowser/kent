table narrowPeak
"peaks of signal enrichment based on pooled, normalized (interpreted) data. It is a BED6+3 format"
(
    string chrom;        "Name of the chromosome"
    uint   chromStart;   "Start position in chromosome"
    uint   chromEnd;     "End position in chromosome"
    string name;	 "Optional. Name given to a region (preferably unique). Use . if no name is assigned."
    uint   score;        "Optional. Indicates how dark the peak will be displayed in the browser (1-1000). If '0', the DCC will assign this based on signal value. Ideally average signalValue per base spread between 100-1000."
    char[2]   strand;       "Optional. +/- to denote strand or orientation (whenever applicable). Use '.' if no orientation is assigned."
    float  signalValue;  "Measurement of average enrichment for the region"
    float  pValue;       "Statistical significance of signal value (-log10)"
    int    peak;         "Point-source called for this peak; 0-based offset from chromStart (use -1 if no point-source called)"
)
