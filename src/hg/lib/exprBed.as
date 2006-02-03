table exprBed
"Expression data information"
    (
    string chrom; "Reference sequence chromosome or scaffold"
    uint chromStart; "position in nucleotides where feature starts on chromosome"
    uint chromEnd; "position in nucleotides where featrure stops on chromsome"
    string name; "feature standardized name; can be a gene, exon or other"
    uint size; "Size of the feature, may be useful if we cannot place it"
    uint uniqueAlign; "1 if alignment was a global maximum, 0 otherwise"
    uint score; "Score from pslLayout of best score"
    string hname; "feature human name: can be a gene, exon or other"
    uint numExp; "Number of experiments in this bed"
    string[numExp] hybes; "Name of experimental hybridization performed, often name of tissue used"
    float[numExp] scores; "log of ratio of feature for particular hybridization"
    )
