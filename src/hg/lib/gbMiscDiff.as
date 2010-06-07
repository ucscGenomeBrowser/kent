table gbMiscDiff
"misc_difference annotations from Genbank"
	(
	char[12] acc;		"Genbank accession"
        int mrnaStart;          "start within mRNA"
        int mrnaEnd;            "end within mRNA"
        lstring notes;          "notes"
        string gene;            "gene name"
        string replacement;     "replace"
	)
