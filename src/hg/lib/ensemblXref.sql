# This table stores cross-reference info between the transcript entries and 
# translation entries of Ensembl genes.  This table is a direct copy of the
# gene_xref table from Ensembl 
CREATE TABLE ensemblXref (
  db varchar(40) NOT NULL default '',		# Ensembl database ?
  analysis varchar(40) NOT NULL default '',	# analysis ?
  type varchar(40) NOT NULL default '',		# type ?
  gene_id int(10) unsigned NOT NULL default '0',	# Gene ID
  gene_name varchar(40) NOT NULL default 'unknown',	# Gene name
  transcript_id int(10) unsigned NOT NULL default '0',	# Transcript ID
  transcript_name varchar(40) NOT NULL default '',	# Transcript name
  translation_id int(10) unsigned NOT NULL default '0',	# Translation ID
  translation_name varchar(40) NOT NULL default '',	# Translation name
  external_db varchar(40) NOT NULL default '',		# External database
  external_name varchar(40) NOT NULL default '',	# External status
  external_status varchar(10) NOT NULL default '',
  KEY gene_id (gene_id,transcript_id),
  KEY db (db,gene_id),
  KEY db_2 (db,external_db,external_name),
  KEY external_name (external_name),
  KEY i1 (transcript_name),
  KEY i2 (translation_name),
  KEY i11 (transcript_name),
  KEY i22 (translation_name)
) TYPE=MyISAM;

