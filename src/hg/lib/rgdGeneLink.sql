# This table store RGD Genelink info
CREATE TABLE rgdGeneLink (
  id varchar(40) NOT NULL default '',	# RGD ID
  name varchar(40),                     # gene name
  refSeq varchar(40),                   # RefSeq ID
  PRIMARY KEY  (id),
  KEY (name),
  KEY (refSeq)
) TYPE=MyISAM;

