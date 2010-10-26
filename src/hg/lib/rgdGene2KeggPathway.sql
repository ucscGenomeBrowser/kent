CREATE TABLE rgdGene2KeggPathway (
  rgdId varchar(40) NOT NULL default '',
  locusID varchar(40) NOT NULL default '',
  mapID varchar(40) NOT NULL default '',
  KEY rgdId (rgdId),
  KEY locusID (locusID),
  KEY mapID (mapID)
) TYPE=MyISAM;

