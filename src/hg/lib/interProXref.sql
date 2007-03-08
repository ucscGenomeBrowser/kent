CREATE TABLE interProXref (
  accession varchar(40) NOT NULL default '',
  method varchar(40) NOT NULL default '',
  start int(8) NOT NULL default '0',
  end int(8) NOT NULL default '0',
  interProId varchar(40) NOT NULL default '',
  description varchar(255) NOT NULL default '',
  KEY accession (accession),
  KEY interProId (interProId)
) TYPE=MyISAM;

