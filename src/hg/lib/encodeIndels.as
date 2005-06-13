table encodeIndels
"ENCODE Deletion and Insertion Polymorphisms from NHGRI"
   (
   string           chrom;       "Chromosome"
   uint             chromStart;  "Start position in chromosome"
   uint             chromEnd;    "End position in chromosome"
   string           name;        "Trace sequence"
   uint             score;       "Quality score "
   char[1]          strand;      "Value should be + or -"
   string           traceName;   "Name of trace"
   string           traceId;     "Trace Id (always integer)"
   uint             tracePos;    "Position in trace"
   char[1]          traceStrand; "Value should be + or -"
   string           reference;   "Reference sequence"
   )
