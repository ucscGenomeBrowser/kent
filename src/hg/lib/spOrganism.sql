CREATE TABLE spOrganism (
  displayID varchar(40) NOT NULL default '',
  organism varchar(40) NOT NULL default '',
  KEY displayID (displayID),
  KEY organism (organism)
) TYPE=MyISAM;

