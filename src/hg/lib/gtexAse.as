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
    uint itemRgb;       "Color based on median ASE value"
    float medianASE;    "Allelic imbalance (0-.5) median"
    float coverage;     "RNA-seq reads overlapping this position (median)"
    uint samples;       "Sample count"
    uint donors;        "Donor count"
    float minASE;       "Minimum ASE"
    float q1ASE;        "Q1 ASE"
    float q3ASE;        "Q3 ASE"
    float maxASE;       "Maximum ASE"
    float stdASE;       "ASE standard deviation"
    )
