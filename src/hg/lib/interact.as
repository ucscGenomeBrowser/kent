table interact
"Interaction between two regions"
    (
    string chrom;      "Chromosome (or contig, scaffold, etc.). For interchromosomal, use 2 records"
    uint chromStart;   "Start position in chromosome of lower region"
    uint chromEnd;     "End position in chromosome of upper region. For interchromosomal, set to chromStart+1"
    string name;       "Name of item, for display.  Usually 'name1/name2' or empty"
    uint score;        "Score from 0-1000"
    char[1] strand;    "Direction of interaction (+ or -)"
    uint color;        "Item color, as itemRgb in bed9"

    string chrom1;     "Chromosome of first region"
    uint chromStart1;  "Start position in chromosome of first region"
    uint chromEnd1;    "End position in chromosome of first region"
    string name1;      "Identifier of first region. Can be used as link to related table"

    string chrom2;     "Chromosome of second region"
    uint chromStart2;  "Start position in chromosome of second region"
    uint chromEnd2;    "End position in chromosome of second region"
    string name2;      "Identifier of second region. Can be used as link to related table"
    )
