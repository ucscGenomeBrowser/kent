table bigPsl
"bigPsl pairwise alignment"
   (
   string chrom;       "Reference sequence chromosome or scaffold"
   uint   chromStart;  "Start position in chromosome"
   uint   chromEnd;    "End position in chromosome"
   string name;        "Name or ID of item, ideally both human readable and unique"
   uint score;         "Score (0-1000)"
   char[1] strand;     "+ or - for strand"
   uint thickStart;    "Start of where display should be thick (start codon)"
   uint thickEnd;      "End of where display should be thick (stop codon)"
   uint reserved;       "RGB value (use R,G,B string in input file)"
   int blockCount;     "Number of blocks"
   int[blockCount] blockSizes; "Comma separated list of block sizes"
   int[blockCount] chromStarts; "Start positions relative to chromStart"

   string  oChrom;     "Other chromosome in the alignment"
   uint    oChromStart;"Start position in other chromosome"
   uint    oChromEnd;  "End position in other chromosome"
   char[1] oStrand;    "+ or - for other strand"
   uint    oChromSize; "Size of other chromosome."
   int[blockCount] oChromStarts; "Start positions relative to oChromStart"

   uint  match;        "Number of bases matched."
   uint matches;       "Number of matching bases that aren't repeats."
   uint misMatches;    "Number of bases that don't match."
   uint repMatches;    "Number of matching bases that are part of repeats."
   uint nCount;        "Number of 'N' bases."
   uint qNumInsert;    "Number of inserts in query."
   uint qBaseInsert;   "Number of bases inserted into query."
   uint tNumInsert;    "Number of inserts in target."
   uint tBaseInsert;   "Number of bases inserted into target."
   )

