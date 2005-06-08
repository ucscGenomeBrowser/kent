table encodeIndels
"ENCODE Deletion and Insertion Polymorphisms from NHGRI"
   (
   string           chrom;       "Chromosome"
   uint             chromStart;  "Start position in chromosome"
   uint             chromEnd;    "End position in chromosome"
   string           name;        "Trace sequence"
   uint             score;       "Quality score "
   char[1]          strand;      "Value should be + or -"
   string           name2;       "Name 2"
   string           traceId;     "Trace Id (always integer)"
   uint             tracePos;    "Position in trace"
   string           refSeq;      "Reference sequence"
   )
