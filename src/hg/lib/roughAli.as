table roughAli
"A rough alignment - not detailed"
   (
   string chrom;      	"Reference sequence chromosome or scaffold"
   uint   chromStart; 	"Start position in chromosome"
   uint   chromEnd;   	"End position in chromosome"
   string name;       	"Name of other sequence"
   uint   score;        "Score from 0 to 1000"
   char[1] strand;      "+ or -"
   uint otherStart;	"Start in other sequence"
   uint otherEnd;	"End in other sequence"
   )

