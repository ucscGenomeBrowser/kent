CREATE TABLE spSecondaryID (
  displayID varchar(40) NOT NULL default '',
  accession varchar(40) NOT NULL default '',
  accession2 varchar(40) NOT NULL default '',
  KEY displayID (displayID),
  KEY accession (accession),
  KEY accession2 (accession2)
) ENGINE=MyISAM;

