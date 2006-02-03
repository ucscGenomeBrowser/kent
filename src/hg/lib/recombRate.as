table recombRate
"Describes the recombination rate in 1Mb intervals based on deCODE map"
   (
   string chrom;    "Reference sequence chromosome or scaffold"
   uint   chromStart;  "Start position in genoSeq"
   uint   chromEnd;    "End position in genoSeq"
   string name;       "Constant string recombRate"

   float   decodeAvg;    "Calculated deCODE recombination rate"
   float   decodeFemale; "Calculated deCODE female recombination rate"
   float   decodeMale;   "Calculated deCODE male recombination rate"
   float   marshfieldAvg;    "Calculated Marshfield recombination rate"
   float   marshfieldFemale; "Calculated Marshfield female recombination rate"
   float   marshfieldMale;   "Calculated Marshfield male recombination rate"
   float   genethonAvg;    "Calculated Genethon recombination rate"
   float   genethonFemale; "Calculated Genethon female recombination rate"
   float   genethonMale;   "Calculated Genethon male recombination rate"
   )
