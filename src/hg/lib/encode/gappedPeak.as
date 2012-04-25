table gappedPeak
"This format is used to provide called regions of signal enrichment based on pooled, normalized (interpreted) data where the regions may be spliced or incorporate gaps in the genomic sequence. It is a BED12+3 format."
    (
    string chrom;	"Reference sequence chromosome or scaffold"
    uint chromStart;	"Pseudogene alignment start position"
    uint chromEnd;      "Pseudogene alignment end position"
    string name;        "Name of pseudogene"
    uint score;          "Score of pseudogene with gene (0-1000)"
    char[1] strand;     "+ or - or . for unknown"
    uint thickStart;    "Start of where display should be thick (start codon)"
    uint thickEnd;      "End of where display should be thick (stop codon)"
    uint reserved;      "Always zero for now"
    int blockCount;     "Number of blocks"
    int[blockCount] blockSizes; "Comma separated list of block sizes"
    int[blockCount] chromStarts; "Start positions relative to chromStart"
    float  signalValue;  "Measurement of average enrichment for the region"
    float  pValue;       "Statistical significance of signal value (-log10). Set to -1 if not used."
    float  qValue;       "Statistical significance with multiple-test correction applied (FDR). Set to -1 if not used."
    )
