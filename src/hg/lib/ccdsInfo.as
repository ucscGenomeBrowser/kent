table ccdsInfo 
"Consensus CDS information, links CCDS ids to NCBI and Hinxton accessions"
	(
	char[12] ccds;		"CCDS id"
        char[1] srcDb;          "source database: N=NCBI, H=Hinxton"
	char[18] mrnaAcc;       "mRNA accession (NCBI or Hinxton)"
	char[18] protAcc;       "protein accession (NCBI or Hinxton)"
	)
