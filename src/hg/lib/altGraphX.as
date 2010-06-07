table altGraphX
"An alternatively spliced gene graph."
    (
    string tName;                   "name of target sequence, often a chrom."
    int tStart;                     "First bac touched by graph."
    int tEnd;                       "Start position in first bac."
    string name;                    "Human readable name."
    uint id;                        "Unique ID."	
    char[2] strand;                 "+ or - strand."
    uint vertexCount;               "Number of vertices in graph."
    ubyte[vertexCount] vTypes;      "Type for each vertex."
    int[vertexCount] vPositions;    "Position in target for each vertex."
    uint edgeCount;                 "Number of edges in graph."
    int[edgeCount] edgeStarts;      "Array with start vertex of edges."
    int[edgeCount] edgeEnds;        "Array with end vertex of edges."
    table evidence[edgeCount] evidence;  "array of evidence tables containing references to mRNAs that support a particular edge."
    int[edgeCount] edgeTypes;       "Type for each edge, ggExon, ggIntron, etc."
    int mrnaRefCount;               "Number of supporting mRNAs."
    string[mrnaRefCount] mrnaRefs;  "Ids of mrnas supporting this." 
    int[mrnaRefCount] mrnaTissues;  "Ids of tissues that mrnas come from, indexes into tissue table"
    int[mrnaRefCount] mrnaLibs;     "Ids of libraries that mrnas come from, indexes into library table"
   )

object evidence
"List of mRNA/ests supporting a given edge"
(
    int evCount;                   "number of ests evidence"
    int [evCount] mrnaIds;         "ids of mrna evidence, indexes into altGraphx->mrnaRefs"
)

