table cytoBand
"Describes the positions of cytogenetic bands with a chromosome"
   (
   string chrom;    "Human chromosome number"
   uint   chromStart;  "Start position in genoSeq"
   uint   chromEnd;    "End position in genoSeq"
   string name;       "Name of cytogenetic band"

   string   gieStain; "Giesma stain results"
   uint   firmStart;   "Start position of part we're confident is in band"
   uint   firmEnd;     "End position of part we're confident is in band"
   )
