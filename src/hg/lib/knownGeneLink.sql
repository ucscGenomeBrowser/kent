CREATE TABLE knownGeneLink (
  name varchar(40) NOT NULL default '',
  seqType char(1) NOT NULL default 'u',
  proteinID varchar(40) NOT NULL default '',
  KEY name (name(10)),
  KEY proteinID (proteinID(10))
) TYPE=MyISAM;

