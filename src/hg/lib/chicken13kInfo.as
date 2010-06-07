table chicken13kInfo
"Extra information for the chicken 13k probes."
(
   string id;           "ID of the probe"
   string source;       "Source of the probe"
   char[1] pcr;         "PCR Amp Code"
   string library;      "Library"
   string clone;        "Source Clone Name"
   string gbkAcc;       "Genbank accession"
   string blat;         "BLAT alignments"
   lstring sourceAnnot; "Source assigned annotation"
   string tigrTc;       "TIGR assigned TC"
   lstring tigrTcAnnot; "TIGR TC annotation"
   lstring blastAnnot;  "BLAST determined annotation"
   lstring comment;     "Comment"
)
