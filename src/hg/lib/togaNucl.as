table TOGANucl
"TOGA nucleotide alignment"
    (
    string chrom;      "Chromosome (or contig, scaffold, etc.)"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of item"

    int exon_num;         "exon number"
    string exon_region;   "region where exon was detected"
    float pid;            "nucleotide %id"
    float blosum;         "normalized blosum score"
    ubyte gaps;           "are there any asm gaps near? 1 - yes 0 - no"
    string ali_class;     "alignemnt class: A, B, C, A+"
    string exp_region;    "where exon was expected"
    ubyte in_exp_region;  "detected in expected region or not 1 yes 0 no"
    lstring alignment;    "exon sequence in query"
    )
