CREATE TABLE cgapAlias (
  cgapID varchar(40) NOT NULL default '',
  alias varchar(80) NOT NULL default '',
  KEY cgapID (cgapID),
  KEY alias (alias)
) ENGINE=MyISAM;

