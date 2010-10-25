# This table store protein sequences of the RGD Genes track entries
CREATE TABLE rgdGene2Pep (
  name varchar(255) NOT NULL default '',	# RGD Gene entry ID
  seq longblob NOT NULL,			# protein sequence of the corresponding RGD Gene
  KEY  (name(32))
) TYPE=MyISAM;

