CREATE TABLE sfDescription (
  name varchar(40) NOT NULL default '',
  proteinID varchar(40) NOT NULL default '',
  description varchar(255) NOT NULL default '',
  KEY name (name),
  KEY protein (proteinID)
) TYPE=MyISAM;

