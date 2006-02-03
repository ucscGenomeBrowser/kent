table scoredRef
"A score, a range of positions in the genome and a extFile offset"
   (
   string chrom;      	"Reference sequence chromosome or scaffold"
   uint   chromStart; 	"Start position in chromosome (forward strand)"
   uint   chromEnd;   	"End position in chromosome"
   uint   extFile;	"Pointer to associated external file"
   bigint offset;	"Offset in external file"
   float score;		"Overall score"
   )


