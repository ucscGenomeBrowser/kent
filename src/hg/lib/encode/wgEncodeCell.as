table wgEncodeCell
"Cell types used by ENCODE (2007-2012)"
(
	uint id; "internal id"
	string term; "public identifier"
	string description; "descriptive phrase"
	string tissue; "organ, anatomical site"
        string type; "tissue, cell line, or differentiated"
        string cancer; "cancer or normal"
        string developmental; "cell lineage"
        string sex; "M, F, or U for unknown"
        string vendor; "name of vendor or lab provider"
        string vendorId; "vendor product code or identifier"
        string url; "order URL or protocol document"
        string ontoTerm; "ontology term"
        string btOntoTerm; "ontology term from Brenda Tissue Ontology"
        string donor; "donor accession at encodeproject.org"
)
