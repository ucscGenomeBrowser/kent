table lsSnpPdb
"Mapping of SNPs to protein structures obtained from LS-SNP PDB"
	(
	string protId;		"Protein Id"
	string pdbId;		"PDB id"
        enum('XRay','NMR') structType;  "type of structure"
	char chain;		"PDB chain id"
        string snpId;           "dbSNP id"
        int snpPdbLoc;          "location of SNP in PDB chain (1-based AA index)"
	)
