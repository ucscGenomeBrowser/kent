table knownCds
"The CDS part of a genePredExt."
    (
    string name;	"Name of gene"
    string cdsStartStat; "Status of CDS start annotation (none, unknown, incomplete, or complete)" 
    string cdsEndStat;   "Status of CDS end annotation (none, unknown, incomplete, or complete)"
    uint exonCount;      "number of exons"
    int[exonCount] exonFrames; "Exon frame {0,1,2}, or -1 if no frame for exon"
    )

