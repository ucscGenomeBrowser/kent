table gtexAse
"BED9+ with additional fields for ASE, coverage, and sample count
    (
    string chrom;       "Reference sequence chromosome or scaffold"
    uint   chromStart;  "Start position in chromosome"
    uint   chromEnd;    "End position in chromosome"
    string name;        "SNP Id"
    uint   score;       "Score from 0-1000; derived from median ASE level"
    char[1] strand;     "n/a"
    uint thickStart;    "n/a"
    uint thickEnd;      "n/a"
    uint rgb;           "Color by ASE value"
    float ASE;          "Allelic imbalance (0-.5)"
    float coverage;     "RNA-seq reads overlapping this position"
    uint samples;       "Sample count"
    )
