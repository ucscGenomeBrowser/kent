table bedRrbs
"BED9+2  Number of reads + percent methylation"
    (
    string chrom;      "Reference chromosome or scaffold"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of item"
    uint   score;      "Score from 0-1000. Capped number of reads"
    char[1] strand;    "+ or - or . for unknown"
    uint thickStart;   "Start of where display should be thick (start codon)"
    uint thickEnd;     "End of where display should be thick (stop codon)"
    uint reserved;     "Color value R,G,B"
    uint readCount;    "Number of reads or coverage"
    uint percentMeth;  "Percentage of reads that show methylation at this position in the genome"
    )
