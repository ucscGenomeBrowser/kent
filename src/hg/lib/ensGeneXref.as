table ensGeneXref
"This is the table gene_xref downloaded directly from Ensembl ftp site (18.34 release 11/04). Please refer to Ensembl site for details.  CAUTION: Ensembl sometimes changes its table definitions and some field may not contais data as its name indicates, e.g. translation_name."
   (
   string db;			#
   string analysis;		#
   string type;			#
   int gene_id;			#
   string gene_name;		#
   int[5] gene_version;		#
   int transcript_id;		#
   string transcript_name;	#
   int[5] transcript_version;	#
   int[5] translation_name;	#
   int translation_id;		#
   int[5] translation_version;	#
   string external_db;		#
   string external_name;	#
   char[10] external_status;	#
   )
