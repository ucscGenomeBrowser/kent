table meiEul1dbRef
"euL1db: L1-HS copies present in the human reference genome"
(
string  chrom;        "Reference chromosome"
uint    chromStart;   "0-based start of reference L1HS element"
uint    chromEnd;     "Half-open end of reference L1HS element"
string  name;         "Subgroup label (L1HS-Ta, L1HS-PreTa, L1HS-Hybrid)"
uint    score;        "Score (unused, 0)"
char[1] strand;       "Strand"
uint    thickStart;   "Start of thick drawing region"
uint    thickEnd;     "End of thick drawing region"
uint    itemRgb;      "RGB color, by L1HS subgroup"
string  family;       "Family|Always L1HS"
string  subGroup;     "Sub-group|L1HS-Ta, L1HS-PreTa, L1HS-Hybrid"
string  integrity;    "Integrity|full-length or 5prime-truncated"
uint    refStart;     "Position on L1HS consensus|5' start in L1HS consensus sequence"
uint    refStop;      "Position on L1HS consensus|3' stop in L1HS consensus sequence"
uint    elementLen;   "Element length (bp)"
)
