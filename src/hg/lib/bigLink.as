

table bigLink
"bigLink pairwise alignment"
    (
    string chrom;       "Reference sequence chromosome or scaffold"
    uint   chromStart;  "Start position in chromosome"
    uint   chromEnd;    "End position in chromosome"
    string name;        "Name or ID of item, ideally both human readable and unique"
    uint qStart;        "start of alignment on query sequence"
    )

