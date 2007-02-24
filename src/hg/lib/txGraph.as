table txGraph
"A transcription graph. Includes alt-splicing info."
    (
    string tName;                   "name of target sequence, often a chrom."
    int tStart;                     "First bac touched by graph."
    int tEnd;                       "Start position in first bac."
    string name;                    "Human readable name."
    char[2] strand;                 "+ or - strand."
    uint vertexCount;               "Number of vertices in graph."
    simple txVertex[vertexCount] vertices;   "Splice sites and soft ends."
    uint edgeCount;                 "Number of edges in graph."
    object txEdge[edgeCount] edgeList; "Edges (introns and exons) in graph."
    int sourceCount;		    "Number of sources of evidence."
    simple txSource[sourceCount] sources; "Sources of evidence."
   )

simple txVertex
"A vertex in a transcription graph - splice site or soft end"
    (
    int position;	"Vertex position in genomic sequence."
    ubyte type;		"Vertex type - ggSoftStart, ggHardStart, etc."
    )

object txEdge
"An edge in a transcription graph - exon or intron"
    (
    int startIx;	"Index of start in vertex array"
    int endIx;		"Index of end in vertex array"
    ubyte type;		"Edge type"
    int evCount;	"Count of evidence"
    object txEvidence[evCount] evList; "List of evidence"
    )

object txEvidence
"Information on evidence for an edge."
    (
    int sourceId;		"Id (index) in sources list"
    int start;		"Start position"
    int end;		"End position"
    )

simple txSource
"Source of evidence in graph."
    (
    string type;   "Type: refSeq, mrna, est, etc."
    string accession;  "GenBank accession. With type forms a key"
    )
