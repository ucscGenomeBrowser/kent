table refAlign
"Contains a region of a reference alignment"
   (
    string chrom;      "Human chromosome or FPC contig"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of item"
    uint   score;      "Score from 0-1000"
    uint matches;      "Number of bases that match"
    uint misMatches;   "Number of bases that don't match"
    uint aNumInsert;   "Number of inserts in aligned seq"
    int aBaseInsert;   "Number of bases inserted in query"
    uint hNumInsert;   "Number of inserts in human"
    int hBaseInsert;   "Number of bases inserted in human"
    string humanSeq;   "Human sequence, contains - for aligned seq inserts"
    string alignSeq;   "Aligned sequence, contains - for human seq inserts"
   ) 
