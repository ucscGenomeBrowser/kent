table altSpliceSite
"Structre to hold information about one splice site in graph."
(
string chrom;                "Chromosome."
uint chromStart;             "Chrom start."
uint chromEnd;               "End."
string agName;               "Name of altGraphX that edge is from. Memory not owned here."
char[2] strand;              "Strand."
uint index;                  "Index into altGraphX records."
uint type;                   "Type of splice site i.e. ggHardStart,ggHardEnd."
uint altCount;               "Number of alternative ways out of splice site."
uint[altCount] vIndexes;      "Index into altGraphX record for alt vertexes."
uint[altCount] altStarts;    "Chromosome starts of alternative outs."
uint[altCount] altTypes;     "Types of vertexes connecting to."
uint[altCount] spliceTypes;  "Types of of splice sites."
uint[altCount] support;      "Number of mRNAs supporting this edge."
)
