table geneFinder
"A gene finding program."
   (
   uint id;	"Unique ID"
   string name;  "Name of gene finder"
   )

table hgGene
"A gene prediction"
    (
    uint id;          "Unique ID"
    string name;      "Name of gene"
    uint geneFinder;  "Program that made prediction"
    uint startBac;    "Bac this starts in"
    uint startPos;    "Position within bac where starts"
    uint endBac;      "Bac this ends in"
    uint endPos;      "Position withing bac where ends"
    byte orientation; "Orientation relative to start bac"
    uint transcriptCount; "Number of transcripts"
    uint[transcriptCount] transcripts; "Array of transcripts"
    )

table hgTranscript
"A transcript prediction"
    (
    uint id;          "Unique ID"
    string name;      "Name of transcript"
    uint hgGene;        "Gene this is in"
    uint startBac;    "Bac this starts in"
    uint startPos;    "Position within bac where starts"
    uint endBac;      "Bac this ends in"
    uint endPos;      "Position withing bac where ends"
    uint cdsStartBac; "Start of coding region."
    uint cdsStartPos; "Start of coding region."
    uint cdsEndBac;   "End of coding region."
    uint cdsEndPos;   "End of coding region."
    byte orientation; "Orientation relative to start bac"
    uint exonCount;   "Number of exons"
    uint[exonCount] exonStartBacs;  "Exon start positions"
    uint[exonCount] exonStartPos;  "Exon start positions"
    uint[exonCount] exonEndBacs;  "Exon start positions"
    uint[exonCount] exonEndPos;  "Exon start positions"
    )
