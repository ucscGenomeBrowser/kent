CREATE TABLE sfAssign (
  genomeID varchar(255) NOT NULL default '',
  seqID varchar(255) NOT NULL default '',
  modelID varchar(255) NOT NULL default '',
  region varchar(255) NOT NULL default '',
  eValue varchar(255) NOT NULL default '',
  sfID varchar(255) NOT NULL default '',
  sfDesc varchar(255) NOT NULL default '',
  KEY sfAssignSeqID (seqID)
) TYPE=MyISAM;

