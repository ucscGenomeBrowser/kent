CREATE TABLE kgPutBack2 (
  acc varchar(40) NOT NULL default '',
  attr varchar(20),
  description blob,
  KEY acc (acc)
) TYPE=MyISAM;

