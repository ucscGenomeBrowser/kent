table interact
"Interaction between two regions"
    (
    string chrom;      "Chromosome (or contig, scaffold, etc.). For interchromosomal, use 2 records"
    uint chromStart;   "Start position in chromosome of lower region. For interchromosomal, set to chromStart of this region"
    uint chromEnd;     "End position in chromosome of upper region. For interchromosomal, set to chromEnd of this region"
    string name;       "Name of item, for display.  Usually 'name1/name2' or empty"
    uint score;        "Score from 0-1000."

    double value;      "Strength of interaction or other data value. Typically basis for score"
    string exp;        "Experiment name (metadata for filtering). Use . if not applicable"
    uint color;        "Item color, as itemRgb in bed9. Typically based on strength or filter"

    string sourceChrom;  "Chromosome of source region (directional) or lower region. For non-directional interchromosomal, chrom of this region."
    uint sourceStart;  "Start position in chromosome of source/lower/this region"
    uint sourceEnd;    "End position in chromosome of source/lower/this region"
    string sourceName;  "Identifier of source/lower/this region. Can be used as link to related table"
    string sourceStrand; "Orientation of source/lower/this region: + or -.  Use . if not applicable"

    string targetChrom; "Chromosome of target region (directional) or upper region. For non-directional interchromosomal, chrom of other region"
    uint targetStart;  "Start position in chromosome of target/upper/this region"
    uint targetEnd;    "End position in chromosome of target/upper/this region"
    string targetName; "Identifier of target/upper/this region. Can be used as link to related table"
    string targetStrand; "Orientation of target/upper/this region: + or -.  Use . if not applicable"
    )
