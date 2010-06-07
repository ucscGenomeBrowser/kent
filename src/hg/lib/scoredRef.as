table scoredRef
"Score and reference to external file position (generally used for multiple alignment display)."
   (
   string chrom;      	"Reference sequence chromosome or scaffold"
   uint   chromStart; 	"Start position in chromosome (forward strand)"
   uint   chromEnd;   	"End position in chromosome"
   uint   extFile;	"ID of associated external file"
   bigint offset;	"Offset in external file"
   float score;		"Overall score"
   )


