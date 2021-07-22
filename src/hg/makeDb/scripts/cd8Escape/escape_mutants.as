table iedb
"iedb bed9+ track"
   (
   string chrom;       "Reference sequence chromosome or scaffold"
   uint   chromStart;  "Start position in chromosome"
   uint   chromEnd;    "End position in chromosome"
   string name;        "Name or ID of item, ideally both human readable and unique"
   uint score;         "Score (0-1000)"
   char[1] strand;     "+ or - for strand"
   uint thickStart;    	"Start of where display should be thick (start codon)"
   uint thickEnd;      	"End of where display should be thick (stop codon)"
   uint pom;            "Point of mutation"
   string gene;         "Type of gene the peptide is originating from"
   string pil;          "Probable infection location"
   string aa_change;    "Amino acid change from wild-type to mutant"
   string c_change;     "Codon change from wild type to mutant"
   string mutant_pep;   "Mutant peptide"
   )
