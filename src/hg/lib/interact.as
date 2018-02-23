table interact
"Interaction between two regions"
    (
    string chrom;      "Chromosome (or contig, scaffold, etc.). For interchromosomal, use 2 records"
    uint chromStart;   "Start position in chromosome of lower region"
    uint chromEnd;     "End position in chromosome of upper region. For interchromosomal, set to chromStart+1"
    string name;       "Name of item, for display.  Usually 'name1/name2' or empty"
    uint score;        "Score from 0-1000"

    double value;      "Strength of interaction or other data value. Typically basis for score"
    string exp;        "Experiment name (metadata for filtering) or empty.
    uint color;        "Item color, as itemRgb in bed9. Typically based on strenght or filter"

    string sourceChrom;  "Chromosome of source region (directional) or lower region."
    uint sourceStart;  "Start position in chromosome of source/lower region"
    uint sourceEnd;    "End position in chromosome of source/lower region"
    string sourceName;  "Identifier of source/lower region. Can be used as link to related table"

    string targetChrom; "Chromosome of target region (directional) or upper region"
    uint targetStart;  "Start position in chromosome of target/upper region"
    uint targetEnd;    "End position in chromosome of target/upper region"
    string targetName; "Identifier of target/upper region. Can be used as link to related table"
    )
