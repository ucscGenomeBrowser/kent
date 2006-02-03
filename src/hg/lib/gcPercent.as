table gcPercent
"Displays GC percentage in 20Kb blocks for genome"
   (
   string chrom;      "Reference sequence chromosome or scaffold"
   uint   chromStart;  "Start position in genoSeq"
   uint   chromEnd;    "End position in genoSeq"
   string name;       "Constant string GCpct"
   uint   gcPpt;      "GC percentage for 20Kb block"
   )
