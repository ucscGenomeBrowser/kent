# This table stores mRNA sequences for Known Genes track entries
CREATE TABLE knownGeneMrna (
  name varchar(255) NOT NULL default '',	# Known Gene entry ID
  seq longblob NOT NULL,			# mRNA sequence
  PRIMARY KEY  (name(32))
) TYPE=MyISAM;

