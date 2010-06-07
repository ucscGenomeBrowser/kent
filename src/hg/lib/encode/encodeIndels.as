table encodeIndels
"ENCODE Deletion and Insertion Polymorphisms from NHGRI"
   (
   string           chrom;       "Reference sequence chromosome or scaffold"
   uint             chromStart;  "Start position in chromosome"
   uint             chromEnd;    "End position in chromosome"
   string           name;        "Trace sequence"
   uint             score;       "Quality score "
   char[1]          strand;      "Always +"
   uint             thickStart;  "Start position in chromosome"
   uint             thickEnd;    "End position in chromosome"
   uint             reserved;    "Reserved"
   string           traceName;   "Name of trace"
   string           traceId;     "Trace Id (always integer)"
   uint             tracePos;    "Position in trace"
   char[1]          traceStrand; "Value should be + or -"
   string           variant;   	 "Variant sequence"
   string           reference;   "Reference sequence"
   )
