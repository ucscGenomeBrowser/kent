table knownGeneLink 
"Known Genes Link table, currently storing DNA based entries only."
   (
   string name; 	"Known Gene ID"
   char[1] seqType;	"sequence type, 'g' genomic, 'm' mRNA, etc."
   string proteinID; 	"Protein ID"
   )
