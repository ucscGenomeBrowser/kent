# This table stores cross-reference info between the transcript entries and 
# translation entries of Ensembl genes.  
CREATE TABLE ensemblXref2 (
  transcript_name varchar(40) NOT NULL default '',	# Transcript name
  translation_name varchar(40) NOT NULL default '',	# Translation name
  KEY i1 (transcript_name),
  KEY i2 (translation_name),
) TYPE=MyISAM;

