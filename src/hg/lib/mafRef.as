table mafRef
"High level information about a multiple alignment. Link to details in maf file."
   (
   string chrom;      	"Chromosome (this species)"
   uint   chromStart; 	"Start position in chromosome (forward strand)"
   uint   chromEnd;   	"End position in chromosome"
   uint   extFile;	"Pointer to associated MAF file"
   bigint offset;	"Offset in MAF file"
   )


