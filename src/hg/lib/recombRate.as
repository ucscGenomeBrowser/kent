table recombRate
"Describes the recombination rate in 1Mb intervals based on deCODE map"
   (
   string chrom;    "Human chromosome number"
   uint   chromStart;  "Start position in genoSeq"
   uint   chromEnd;    "End position in genoSeq"
   string name;       "Constant string recombRate"

   float   recombRate; "Calculated recombination rate for this interval"
   )
