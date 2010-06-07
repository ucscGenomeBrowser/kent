CREATE TABLE gsIdXref (
  subjId varchar(40) NOT NULL default '',
  dnaSeqId varchar(40) NOT NULL default '',
  aaSeqId varchar(40) NOT NULL default '',
  KEY subjId (subjId),
  KEY dnaSeqId (dnaSeqId),
  KEY aaSeqId (aaSeqId)
) TYPE=MyISAM;

