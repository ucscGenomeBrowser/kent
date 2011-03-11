table bedRrbs
"BED9 +  number of reads + percent methylation"
    (
    string chrom;      "Chromosome (or contig, scaffold, etc.)"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of item"
    uint   score;      "Score from 0-1000. Capped number of reads"
    char[1] strand;    "+ or -"
    uint thickStart;   "Start of where display should be thick (start codon)"
    uint thickEnd;     "End of where display should be thick (stop codon)"
    uint itemRgb;      "Color value"
    uint readCount;    "number of reads or coverage"
    uint percentMeth;  "percentage of reads that show methylation at this position in the genome"
    )
