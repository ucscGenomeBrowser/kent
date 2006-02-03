table zdobnovSynt
"Gene homology-based synteny blocks from Evgeny Zdobnov/Peer Bork et al."
    (
    string chrom;      "Reference sequence chromosome or scaffold"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of item: "
    uint   score;      "Score placeholder"
    char[1] strand;    "placeholder"
    uint thickStart;   "Start of where display should be thick (start codon)"
    uint thickEnd;     "End of where display should be thick (stop codon)"
    uint reserved;     "Always zero for now"
    int blockCount;    "Number of blocks (syntenic genes)"
    int[blockCount] blockSizes; "Comma separated list of block sizes"
    int[blockCount] chromStarts; "Start positions relative to chromStart"
    string[blockCount] geneNames; "Names of genes (from other species)"
    )
