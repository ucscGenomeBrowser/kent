CREATE TABLE varProtTemp (
  acc varchar(40) NOT NULL default '',
  biodatabaseID int(11) NOT NULL default '0',
  parAcc varchar(40) NOT NULL default '',
  description varchar(255) NOT NULL default '',
  KEY acc(acc)
) TYPE=MyISAM;

