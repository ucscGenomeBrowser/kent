table lowelabPfamHits
"Pfam domain hits mapped to the genome"
  (
  string chrom;      "chromosome"
  uint chromStart;   "Start position in chromosome"
  uint chromEnd;     "End position in chromosome"
  string name;       "hit name"
  uint score;        "Score from 900-1000.  1000 is best"
  char[1] strand;    "Value should be + or -"
  string pfamAC;     "Pfam accession number"
  string pfamID;     "Pfam ID"
  string swissAC;    "SWISS-PROT accession number"
  string protCoord;  "Coordiates of Pfam domain hit within organism protein"
  uint ident;        "Percent identity of Pfam-A seq with this hit (should be >90%)"
  uint percLen;       "Percent length of Pfam domain relative to protein length)"	
  )
