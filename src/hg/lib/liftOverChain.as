table liftOverChain
"Chain file for lifting annotations between assemblies"
    (
    string  fromDb;	"Short name of 'from' database.  'hg15' or the like"
    string  toDb;	"Short name of 'to' database.  'hg16' or the like"
    lstring path;	"Path to chain file"
    float minMatch;     "Minimum ratio of bases that must remap."
    uint minSizeT;      "Minimum chain size in target. Should be renamed to minChainT"  
    uint minSizeQ;      "Minimum chain size in query."
    char[1] multiple;   "Use -multiple by default."
    float minBlocks;    "Min ratio of alignment blocks/exons that must map."
    char[1] fudgeThick; "If thickStart/thickEnd is not mapped, use the closest mapped base."
    )
