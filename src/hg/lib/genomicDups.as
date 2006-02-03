table genomicDups
"Summary of large genomic Duplications (>1KB >90% similar)"
  (
  string   chrom;       "Reference sequence chromosome or scaffold"
  uint    chromStart;   "Start position in chromosome"
  uint    chromEnd;     "End position in chromosome"
  string  name;         "Other FPC contig involved"
  uint    score;        "Score from 900-1000.  1000 is best"
  char[1] strand;       "Value should be + or -"
  string  otherChrom;   "Other chromosome or scaffold"
  uint    otherStart;   "Start in other sequence"
  uint    otherEnd;     "End in other sequence"
  uint    alignB;       "bases Aligned"
  uint    matchB;       "aligned bases that match"
  uint    mismatchB;    "aligned bases that do not match"
  float   fracMatch;    "fraction of matching bases"
  float   jcK;          "K-value calculated with Jukes-Cantor"
  )
