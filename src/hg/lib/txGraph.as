table txGraph
"A transcription graph. Includes alt-splicing info."
    (
    string tName;                   "name of target sequence, often a chrom."
    int tStart;                     "First bac touched by graph."
    int tEnd;                       "Start position in first bac."
    string name;                    "Human readable name."
    char[2] strand;                 "+ or - strand."
    uint vertexCount;               "Number of vertices in graph."
    ubyte[vertexCount] vTypes;      "Type for each vertex."
    int[vertexCount] vPositions;    "Position in target for each vertex."
    uint edgeCount;                 "Number of edges in graph."
    int[edgeCount] edgeStarts;      "Array with start vertex of edges."
    int[edgeCount] edgeEnds;        "Array with end vertex of edges."
    object txEvList[edgeCount] evidence;  "array of evidence tables containing references to mRNAs that support a particular edge."
    int[edgeCount] edgeTypes;       "Type for each edge, ggExon, ggIntron, etc."
    int sourceCount;		     "Number of sources of evidence."
    object txSource[sourceCount] sources; "Sources of evidence."
   )

object txEvList
"List of mRNA/ests supporting a given edge"
(
    int evCount;                   "number of ests evidence"
    object txEvidence [evCount] evList;         "ids of mrna evidence, indexes into altGraphx->mrnaRefs"
)

object txEvidence
"Information on evidence for an edge."
    (
    int sourceId;		"Id (index) in sources list"
    int start;		"Start position"
    int end;		"End position"
    )

object txSource
"Source of evidence in graph."
    (
    string type;   "Type: refSeq, mrna, est, etc."
    string accession;  "GenBank accession. With type forms a key"
    )
