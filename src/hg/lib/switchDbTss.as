table switchDbTss
"Switchgear Genomics TSS DB table"
(
        string  chrom;          "Reference sequence chromosome or scaffold"
        uint    chromStart;     "Start in Chromosome"
        uint    chromEnd;       "End in Chromosome"
        string  name;           "Name"
        uint    score;          "Score"
        char[1] strand;         "Strand"
	double  confScore;      "Confidence score"
	string  gmName;         "Gene model UID/name"
    	uint	gmChromStart;   "Gene model chromStart"
	uint    gmChromEnd;     "Gene model chromEnd"
	ubyte   isPseudo;       "0 if not a pseudogene TSS, 1 if it is"
)
