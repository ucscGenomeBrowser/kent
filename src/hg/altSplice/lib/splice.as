object path
"List of vertices through a graph that forms a path of interest."
(
   string tName;           "Target name (usually chromosome)."
   int tStart;             "Target start (usually chromStart)."
   int tEnd;               "Target end (usually chromEnd)."  
   int type;               "Used to classify path as major, minor or in between."
   int maxVCount;          "Size of vertices array. Used for memory management."
   int vCount;             "Number of vertices that make up path."
   int [vCount] vertices;  "Array of vertices that make up path."
   int upV;                "Closest upstream vertex connected to vertices[0]."   
   int downV;              "Closest downstream vertex connected vertices[vCount-1]."
   int bpCount;            "Number of exonic base pairs in path."
)

table splice
"One alternative splicing event and the associated paths and sub-splicing events."
(
string tName;                      "Target name (usually chromosome)."
int tStart;                        "Target start (usually chromStart)."
int tEnd;                          "Target end (usually chromEnd)."  
string name;                       "Unique name of splicing event."
int type;                          "Type of alternative splicing event."
char[2] strand;                    "+ or -."
int agxId;                         "Unique id of altGraphX graph associated to this splice."
int vCount;                        "Number of vertices in associated altGraphX graph."
int[vCount] vPositions;            "Local copy of positions of vertices."
ubyte[vCount] vTypes;              "Type for each vertex."	
int pathCount;                     "Number of paths."
table path[pathCount] paths;       "All paths resulting from this splice."
)
