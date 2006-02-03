table rnaGenes
"All rna Genes"
  (
  string chrom;       "Reference sequence chromosome or scaffold"
  uint chromStart;   "Start position in chromosome"
  uint chromEnd;     "End position in chromosome"
  string name;         "Other chromosome involved"
  uint score;        "Score from 900-1000.  1000 is best"
  char[1] strand;       "Value should be + or -"
  string aa;         "Amino acid for the tRNA"
  string ac;         "Anticodon for the tRNA"
  char[1] ci;        "Contains intron or not: Y or N"
  float scan;        "tRNAScanSE score"
  )
