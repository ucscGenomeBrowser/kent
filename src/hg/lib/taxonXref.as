table taxonXref 
"mapping from ucsc organism name to ncbi tax id/name"
(
        string  organism ;        "UCSC organism name"
	uint    taxon ;             "NCBI taxon ID"
	string  name ;              "NCBI taxon name Binomial format "
	string  toGenus ;           "NCBI tree down to Genus"
)
