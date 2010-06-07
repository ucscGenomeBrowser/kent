CREATE TABLE hprdToCdna (
hprdId varchar(255) NOT NULL default '',
cdnaId varchar(255) NOT NULL default '',
KEY hprdId (hprdId(10)),
KEY cdnaId (cdnaId(10))
) TYPE=MyISAM;
