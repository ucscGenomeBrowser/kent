table clsModelBed
"CLS long-read LyRiC models"
    (
    string chrom;      "Chromosome"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Model id"
    uint   score;      "unused"
    char[1] strand;    "+ or -"
    uint thickStart;   "Same as chromStart"
    uint thickEnd;     "Same as chromEnd"
    uint reserved;     "unised"
    int blockCount;    "Number of blocks"
    int[blockCount] blockSizes; "Comma separated list of block sizes"
    int[blockCount] chromStarts; "Start positions relative to chromStart"
    int sampleCount;    "Number of samples containing the isoform"
    string[sampleCount] samples;  "Samples and platforms containing this model"
    string[sampleCount] sampleIds;  "Sample ids"
    )
