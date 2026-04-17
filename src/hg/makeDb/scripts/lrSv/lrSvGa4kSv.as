table lrSvGa4kSv
"Genomic Answers for Kids (GA4K) long-read structural variants"
    (
    string chrom;           "Chromosome"
    uint chromStart;        "Start position"
    uint chromEnd;           "End position"
    string name;            "Variant ID"
    uint score;             "Score"
    char[1] strand;         "Strand"
    uint thickStart;        "Thick start (same as chromStart)"
    uint thickEnd;          "Thick end (same as chromEnd)"
    uint reserved;          "Item color"
    string svType;          "SV Type|DEL, INS, DUP or INV"
    int svLen;              "SV Length|Length of the structural variant in base pairs (absolute value)"
    float alleleFreq;       "Allele Frequency|Fraction of samples carrying this variant (SVF field)"
    uint carrierCount;      "Carrier Count|Number of samples carrying this variant (SVC field)"
    uint sampleTotal;       "Total Samples|Total number of samples genotyped (SVN field)"
    )
