table gbRNAs
"Genbank RNA genes"
  (
  string chrom;      "Reference sequence chromosome or scaffold"
  uint chromStart;   "Start position in chromosome"
  uint chromEnd;     "End position in chromosome"
  string name;       "gene name"
  uint score;        "Score from 900-1000.  1000 is best"
  char[1] strand;    "Value should be + or -"
  string product;       "Description of RNA gene"
  string intron;       "Coordinates of intron in RNA gene"
  )
