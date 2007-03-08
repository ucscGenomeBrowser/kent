CREATE TABLE spDisease (
  accession varchar(40) NOT NULL default '',
  displayID varchar(40) NOT NULL default '',
  diseaseDesc text,
  KEY accession (accession),
  KEY displayID (displayID)
) TYPE=MyISAM;

