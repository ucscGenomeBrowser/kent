# This table store RGD QTL link info
CREATE TABLE rgdQtlLink (
  id varchar(40) NOT NULL default '',	# RGB ID
  name varchar(40),                 	# name
  description varchar(255),		# description
  PRIMARY KEY  (id),
  KEY (name)
) ENGINE=MyISAM;

