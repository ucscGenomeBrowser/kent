table spliceNode
"A Single splice site node in a splicing graph"
(
    string tName;                   "name of target sequence, often a chrom."
    int tStart;                     "start in tName."
    int tEnd;                       "end in tName."
    char[2] strand;                 "+ or - strand."
    uint id;                        "Unique ID."
    uint type;                      "Type of splice site, see enum ggVertexType."	
    uint class;                     "Number of node in graph, different than ID."
    uint color;                     "Used for algorithm classification."
    uint edgeCount;                 "Number of edges leaving from this graph."
    uint[edgeCount] edges;          "Ids of spliceNodes that this edge connects to."
)

table splicePath
"Path of one EST/mRNA through splieNodes"
(
    string tName;                      "name of target sequence, often a chrom."
    int tStart;                        "start in tName."
    int tEnd;                          "end in tName."
    string qName;                      "Accession of mRNA or EST."
    char[2] strand;                    "+ or - strand."
    string path;                       "String representation of path."
    uint nodeCount;                    "Number of nodes in graph."
    table spliceNode[nodeCount] nodes; "Array of dynamically allocated spliceNodes whose id corresponds to their index in array."
)
	
table spliceGraph
"A collection of spliceNodes forming a graph"
(
    string tName;                      "name of target sequence, often a chrom."
    int tStart;                        "start in tName."
    int tEnd;                          "end in tName."
    char[2] strand;                    "+ or - strand."
    uint classCount;                   "Number of different classes of nodes, two nodes are of same class if at same position."
    uint[classCount] classStarts;      "start in tName."
    uint[classCount] classEnds;        "end in tName."
    uint[classCount] classTypes;       "Type of splice site, see enum ggVertexType."	
    uint nodeCount;                    "Total number of nodes in graph."
    uint nodeCapacity;                 "Maximum nodes allowed without expanding memory."
    uint pathCount;                    "Number of different paths generating graph."
    table splicePath[pathCount] paths; "Array of dynamically allocated splicePaths which make up the graph."
)



	