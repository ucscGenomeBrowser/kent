# This table store protein sequences of the Known Genes track entries
CREATE TABLE knownGenePep (
  name varchar(255) NOT NULL default '',	# Known Gene entry ID
  seq longblob NOT NULL,			# protein sequence of the corresponding Known Gene
  KEY  (name(32))
) TYPE=MyISAM;

