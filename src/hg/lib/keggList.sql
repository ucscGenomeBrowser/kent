#used during KG build
CREATE TABLE keggList (
  locusID varchar(40) NOT NULL default '',
  mapID varchar(40) NOT NULL default '',
  description varchar(255) NOT NULL default '',
  KEY locusID (locusID),
  KEY mapID (mapID)
) TYPE=MyISAM;

