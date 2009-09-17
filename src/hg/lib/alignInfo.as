table alignInfo "Aligned Features Description"
	(
	  string chrom;      "chromosome"
	  uint chromStart;   "Start position in chromosome"
	  uint chromEnd;     "End position in chromosome"
	  string name;       "hit name"
	  uint score;        "Score from 900-1000.  1000 is best"
	  char[1] strand;    "Value should be + or -"
	  uint hasmatch;     "If it has match in aligned species"
	string orgn;      "aligned organism"
	  string alignChrom;      "chromosome for aligned species"
	  uint alignChromStart;   "Start position in chromosome for aligned species"
	  uint alignChromEnd;     "End position in chromosome for aligned species"

	)