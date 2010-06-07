table altGraph 
"An alternatively spliced gene graph."
    (
    uint id;                        "Unique ID"
    byte orientation;               "+1 or -1 relative to start bac"
    uint startBac;               "First bac touched by graph"
    uint startPos;                  "Start position in first bac"
    uint endBac;                 "Last bac touched by graph"
    uint endPos;                    "End position in last bac"
    uint vertexCount;               "Number of vertices in graph"
    ubyte[vertexCount] vTypes;      "Type for each vertex"
    uint[vertexCount] vBacs;     "Bac for each vertex"
    uint[vertexCount] vPositions;   "Position in bac for each vertex"
    uint edgeCount;                 "Number of edges in graph"
    uint[edgeCount] edgeStarts;     "Array with start vertex of edges"
    uint[edgeCount] edgeEnds;       "Array with end vertex of edges"
    int mrnaRefCount;               "Number of supporting mRNAs"
    uint[mrnaRefCount] mrnaRefs;    "IDs of all supporting mRNAs"
    )

