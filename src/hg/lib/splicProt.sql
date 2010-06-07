CREATE TABLE splicProt (
  varAcc  varchar(40) NOT NULL default '',
  parAcc  varchar(40) NOT NULL default '',
  variant varchar(4) NOT NULL default '',
  KEY varAcc (varAcc)
) TYPE=MyISAM;

