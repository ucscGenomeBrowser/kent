table estInfo
"Extra information on ESTs - calculated by polyInfo program"
    (
    string chrom;      "Human chromosome or FPC contig"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Accession of EST"
    short intronOrientation; "Orientation of introns with respect to EST"
    short sizePolyA;   "Number of trailing A's"
    short revSizePolyA; "Number of trailing A's on reverse strand"
    short signalPos;   "Position of start of polyA signal relative to end of EST or 0 if no signal"
    short revSignalPos;"PolyA signal position on reverse strand if any"
    )
