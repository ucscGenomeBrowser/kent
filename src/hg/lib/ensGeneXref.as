table ensGeneXref
"This is the table gene_xref downloaded directly from Ensembl ftp site (18.34 release 11/04). Please refer to Ensembl site for details.  CAUTION: Ensembl sometimes changes its table definitions and some field may not contais data as its name indicates, e.g. translation_name."
   (
   string db;			"unknown - check with Ensembl"
   string analysis;		"unknown - check with Ensembl"
   string type;			"unknown - check with Ensembl"
   int gene_id;			"gene ID"
   string gene_name;		"gene name"
   int[5] gene_version;		"gene version"
   int transcript_id;		"transcript  ID"
   string transcript_name;	"transcript  name"
   int[5] transcript_version;	"transcript  version"
   string translation_name;	"translation name"
   int translation_id;		"translation ID"
   int[5] translation_version;	"translation version"
   string external_db;		"external database"
   string external_name;	"external name"
   char[10] external_status;	"external status"
   )
