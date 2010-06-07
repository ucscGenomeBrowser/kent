table arCOGs "arCOG Information"
	(
	  string chrom;      "chromosome"
	  uint chromStart;   "Start position in chromosome"
	  uint chromEnd;     "End position in chromosome"
	  string name;       "hit name"
	  uint score;        "Score from 900-1000.  1000 is best"
	  char[1] strand;    "Value should be + or -"

	string gene;		"standard name"
	)