# used during KG build
CREATE TABLE spRef (
  spID varchar(255) NOT NULL default '',
  mrnaID varchar(255) NOT NULL default '',
  KEY spID (spID),
  KEY mrnaID (mrnaID)
) TYPE=MyISAM;

