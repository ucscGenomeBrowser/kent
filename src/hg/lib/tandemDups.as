table tandemDups
"paired exactly identical sequence up to 20,000 bases separation"
   (
   string chrom;       "Reference sequence chromosome or scaffold"
   uint   chromStart;  "Start position in chromosome"
   uint   chromEnd;    "End position in chromosome"
   string name;        "Name of item. (browser position coordinates in this case)"
   uint score;         "Score (0-1000) (normalized duplicateSize to this range)"
   char[1] strand;     "+ or - for strand"
   uint thickStart;   "Start of where display should be thick (start codon)"
   uint thickEnd;     "End of where display should be thick (stop codon)"
   uint reserved;     "always zero"
   int blockCount;    "Number of blocks"
   int[blockCount] blockSizes; "Comma separated list of block sizes"
   int[blockCount] chromStarts; "Start positions relative to chromStart"
   uint duplicateSize; "size of duplicated sequence"
   )

