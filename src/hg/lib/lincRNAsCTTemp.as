table lincRNAsCTTemp
"lincRNA reads expression table by cell type"
   (
    string chrom; "Human chromosome or FPC contig"
    uint chromStart; "Start position in chromosome"
    uint chromEnd; "End position in chromosome"
    string name; "Name of item"
    uint score; "Score from 0-1000"
    float rawScore; "Raw Signal Score"
    float log2RawScore; "log2 of raw score"
    )
