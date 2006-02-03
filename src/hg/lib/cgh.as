table cgh
"Comparative Genomic Hybridization data assembly position information"
    (
    string chrom; "Reference sequence chromosome or scaffold"
    uint chromStart; "position in nucleotides where feature starts on chromosome"
    uint chromEnd; "position in nucleotides where featrure stops on chromsome"
    string name;  "Name of the cell line (type 3 only)"
    float score; "hybridization score"
    uint type;  "1 - overall average, 2 - tissue average, 3 - single tissue"
    string tissue;  "Type of tissue cell line derived from (type 2 and type 3)"
    string clone; "Name of clone"
    uint spot;  "Spot number on array"
    )
