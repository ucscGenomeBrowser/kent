table altGraph 
"An alternatively spliced gene graph."
    (
    uint id;                        "Unique ID"
    string tName;                   "name of target sequence, often a chrom."
    int tStart;                    "First bac touched by graph"
    int tEnd;                      "Start position in first bac"
    char[2] strand;                 "+ or - strand"
    uint vertexCount;               "Number of vertices in graph"
    ubyte[vertexCount] vTypes;      "Type for each vertex"
    int[vertexCount] vPositions;   "Position in target for each vertex"
    uint edgeCount;                 "Number of edges in graph"
    int[edgeCount] edgeStarts;     "Array with start vertex of edges"
    int[edgeCount] edgeEnds;       "Array with end vertex of edges."
    int mrnaRefCount;               "Number of supporting mRNAs."
    string[mrnaRefCount] mrnaRefs;  "Ids of mrnas supporting this." 
   )

