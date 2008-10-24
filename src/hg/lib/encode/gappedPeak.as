table gappedPeak
"This format is used to provide called regions of signal enrichment based on pooled, normalized (interpreted) data where the regions may be spliced or incorporate gaps in the genomic sequence. It is a BED12+3 format."
    (
    string chrom;	"Reference sequence chromosome or scaffold"
    uint chromStart;	"pseudogene alignment start position"
    uint chromEnd;      "pseudogene alignment end position"
    string name;        "Name of pseudogene"
    uint score;         "score of pseudogene with gene"
    char[2] strand;     "+ or -"
    uint thickStart;    "Start of where display should be thick (start codon)"
    uint thickEnd;      "End of where display should be thick (stop codon)"
    uint reserved;      "Always zero for now"
    int blockCount;     "Number of blocks"
    int[blockCount] blockSizes; "Comma separated list of block sizes"
    int[blockCount] chromStarts; "Start positions relative to chromStart"
    float  signalValue;  "Measurement of average enrichment for the region"
    float  pValue;       "Within dataset statistical significance of signal value (-log10)"
    float  qValue;       "Cross-dataset statistical significance of signal value (-log10)"
    )
