CREATE TABLE ensPep (
  name varchar(255) NOT NULL default '',
  seq longblob NOT NULL,
  PRIMARY KEY  (name(64))
) TYPE=MyISAM;

