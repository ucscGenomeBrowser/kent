CREATE TABLE keggPathway (
  kgID varchar(40) NOT NULL default '',
  locusID varchar(40) NOT NULL default '',
  mapID varchar(40) NOT NULL default '',
  KEY kgID (kgID),
  KEY locusID (locusID),
  KEY mapID (mapID)
) ENGINE=MyISAM;

