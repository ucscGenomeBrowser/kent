table tRNAs
"transfer RNA genes"
  (
  string chrom;      "Reference sequence chromosome or scaffold"
  uint chromStart;   "Start position in chromosome"
  uint chromEnd;     "End position in chromosome"
  string name;       "transfer RNA gene name"
  uint score;        "Score from 900-1000.  1000 is best"
  char[1] strand;       "Value should be + or -"
  string aa;         "Amino acid for the tRNA"
  string ac;         "Anticodon for the tRNA"
  string intron;     "Coordinates for intron"
  float trnaScore;        "tRNAScanSE score"
  )
