table knownCds
"The CDS part of a genePredExt."
    (
    string name;	"Name of gene"
    string cdsStartStat; "enum('none','unk','incmpl','cmpl')"
    string cdsEndStat;   "enum('none','unk','incmpl','cmpl')"
    uint exonCount;      "number of exons"
    int[exonCount] exonFrames; "Exon frame {0,1,2}, or -1 if no frame for exon"
    )

