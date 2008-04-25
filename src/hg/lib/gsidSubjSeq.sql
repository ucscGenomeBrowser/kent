CREATE TABLE gsidSubjSeq (
  subjId varchar(40) NOT NULL default '',
  dnaSeqIds blob,
  aaSeqIds blob,
  KEY subjId (subjId)
) TYPE=MyISAM;

