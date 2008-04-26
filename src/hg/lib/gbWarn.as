table gbWarn
"warnings about particular clones in GenBank"
	(
	char[12] acc;		"Genbank accession"
        enum('invitroNorm', 'athRage', 'orestes') reason; "reason for warning"
	)
