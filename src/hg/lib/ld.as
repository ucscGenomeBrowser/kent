table ld
"Linkage Disequilibrium values from the HapMap project"
    (
    string   chrom;      "Chromosome"
    uint     chromStart; "chromStart for reference marker"
    uint     chromEnd;   "chromEnd for last marker in list"
    string   name;       "rsId"
    string   pop;        "Population"
    uint     ldCount;    "number of markers with LD values"
    string   ldStarts;   "start positions of markers"
    string   dprime;     "D' value list"
    string   rsquared;   "r^2 value list"
    string   lod;        "LOD value list"
    )
