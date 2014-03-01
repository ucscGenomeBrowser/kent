CREATE TABLE bioCycPathway (
  kgID varchar(40) NOT NULL default '',
  geneID varchar(40) NOT NULL default '',
  mapID varchar(40) NOT NULL default '',
  KEY kgID (kgID),
  KEY geneID (geneID),
  KEY mapID (mapID)
) ENGINE=MyISAM;

