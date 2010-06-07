CREATE TABLE spXref3 (
  accession varchar(40) NOT NULL default '',
  displayID varchar(40) NOT NULL default '',
  division varchar(40) NOT NULL default '',
  bioentryID int(11) NOT NULL default '0',
  biodatabaseID int(11) NOT NULL default '0',
  description text NOT NULL default '',
  hugoSymbol varchar(40) NOT NULL default '',
  hugoDesc varchar(255) NOT NULL default '',
  KEY accession(accession),
  KEY displayID(displayID),
  KEY hugoSymbol(hugoSymbol)
) TYPE=MyISAM;

