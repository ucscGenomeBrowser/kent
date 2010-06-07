table jgiGene
"JGI curated genes"
  (
  string chrom;      "Reference sequence chromosome or scaffold"
  uint chromStart;   "Start position in chromosome"
  uint chromEnd;     "End position in chromosome"
  string name;       "JGI locus tag"
  uint score;        "1000"
  char[1] strand;    "Value should be + or -"
  string jgiGeneId;  "JGI gene ID"
  string jgiSymbol;  "JGI gene symbol"
  float jgiGc;       "GC content"
  string jgiDescription;     "ORF annotation"
 )
