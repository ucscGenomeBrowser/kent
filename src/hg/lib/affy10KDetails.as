table affy10KDetails
"Information from affy10KDetails representing the Affymetrix 10K Mapping Array"
(	string     affyId;    "Affymetrix SNP id"
	string     rsId;      "RS identifier (some are null)"
	string     tscId;     "TSC identifier (some are null)"
	char[2]    baseA;     "The first allele (A)"
	char[2]    baseB;     "The second allele (B)"
	char[34]   sequenceA; "The A allele with flanking sequence"
	char[34]   sequenceB; "The B allele with flanking sequence"
	char[8]    enzyme;    "The enzyme that was used to prepare the sample (HindIII or XbaI)"
)
