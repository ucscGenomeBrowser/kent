# This table store RDB SSLP link info
CREATE TABLE rgdSslpLink (
  id varchar(40) NOT NULL default '',	# RGD ID
  name varchar(40),                     # SSLP name
  PRIMARY KEY  (id),
  KEY (name)
) TYPE=MyISAM;

