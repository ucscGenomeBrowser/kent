table exonNode
"A Single exon site node in a splicing graph"
(
    string tName;                   "name of target sequence, often a chrom."
    int tStart;                     "start in tName. 3' splice site"
    int tEnd;                       "end in tName. 5' splice site"
    char[2] strand;                 "+ or - strand."
    uint id;                        "Unique ID."
    uint startClass;                "Class of start, splice site number"
    uint endClass;                  "Class of end, splice site number"
    uint startType;                 "Type of starting splice site, ggVertexType."
    uint endType;                   "Type of ending splice site, ggVertexType."
    uint type;                      "Type of exon, see enum ggEdgeType."	
    int class;                     "Number of node in graph, different than ID."
    uint color;                     "Used for algorithm classification."
    uint startCount;                "If wildcard starts number of starts that can match"
    uint[startCount] starts;        "location of starting positions that match."
    uint endCount;                  "If wildcard end, number of starts that can match"
    uint[endCount] ends;            "location of ending positions that match."
    uint edgeInCount;               "Number of edges inputting into this node."
    uint[edgeInCount] edgesIn;        "Ids of nodes that connect to this node."
    uint edgeOutCount;              "Number of edges leaving from this graph."
    uint[edgeOutCount] edgesOut;        "Ids of exonNodes that this node connects to, by convention edgesOut[0] = next node in path"
)

table exonPath
"Path of one EST/mRNA through exonNodes"
(
    string tName;                      "name of target sequence, often a chrom."
    int tStart;                        "start in tName."
    int tEnd;                          "end in tName."
    string qName;                      "Accession of mRNA or EST."
    char[2] strand;                    "+ or - strand."
    string path;                       "String representation of path."
    uint nodeCount;                    "Number of nodes in graph."
    table exonNode[nodeCount] nodes;    "Array of dynamically allocated exonNodes whose id corresponds to their index in array."
)
	
table exonGraph
"A collection of exonNodes forming a graph"
(
    string tName;                      "name of target sequence, often a chrom."
    int tStart;                        "start in tName."
    int tEnd;                          "end in tName."
    char[2] strand;                    "+ or - strand."
    uint classCount;                   "Number of different classes of nodes, two nodes are of same class if at same position."
    uint[classCount] classStarts;      "start in tName."
    uint[classCount] classEnds;        "end in tName."
    uint[classCount] classTypes;       "Type of exon site, see enum ggVertexType."	
    uint nodeCount;                    "Total number of nodes in graph."
    uint nodeCapacity;                 "Maximum nodes allowed without expanding memory."
    uint pathCount;                    "Number of different paths generating graph."
    table exonPath[pathCount] paths;   "Array of dynamically allocated exonPaths which make up the graph."
)



	