# This table store RGD EST link info
CREATE TABLE rgdEstLink (
  id varchar(40) NOT NULL default '',	# RGD ID
  name varchar(40),                     # EST name
  PRIMARY KEY  (id),
  KEY (name)
) TYPE=MyISAM;

