table encodeErgeExpProm
"ENCODE experimental data from dbERGEII"
   (
   string           chrom;       "Reference sequence chromosome or scaffold"
   uint             chromStart;  "Start position in chromosome"
   uint             chromEnd;    "End position in chromosome"
   string           name;        "Name of read - up to 255 characters"
   uint             score;       "Score from 0-1000.  1000 is best"
   char[1]          strand;      "Value should be + or -"
   uint             thickStart;  "Start of where display should be thick (start codon)"
   uint             thickEnd;    "End of where display should be thick (stop codon)"
   uint             reserved;    "Always zero for now"
   uint             blockCount;  "Number of separate blocks (regions without gaps)"
   uint[blockCount] blockSizes;  "Comma separated list of block sizes"
   uint[blockCount] chromStarts; "Start position of each block in relative to chromStart"
   string           Id;          "dbERGEII Id"
   string           color;       "RGB color values"
   )
