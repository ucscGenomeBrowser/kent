table snoRNAs
"C/D box snoRNA genes"
  (
  string chrom;      "Reference sequence chromosome or scaffold"
  uint chromStart;   "Start position in chromosome"
  uint chromEnd;     "End position in chromosome"
  string name;       "snoRNA RNA gene name"
  uint score;        "Score from 900-1000.  1000 is best"
  char[1] strand;       "Value should be + or -"
  float snoScore;       "snoscan score"
  string targetList;    "Target RNAs"
  string orthologs;     "Possible gene orthologs in closely-related species"
  string guideLen;      "Length of guide region"
  lstring guideStr;       "Guide region string"
  string guideScore;     "Guide region score"
  string cBox;           "C box feature"
  string dBox;           "D box feature"
  string cpBox;           "C' box feature"
  string dpBox;           "D' box feature"
  float hmmScore;         "score to HMM snoRNA model"
  lstring snoscanOutput;   "Full snoscan output for snoRNA"
  )

