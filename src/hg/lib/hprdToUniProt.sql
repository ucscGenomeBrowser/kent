CREATE TABLE hprdToUniProt (
hprdId varchar(255) NOT NULL default '',
uniProtId varchar(255) NOT NULL default '',
KEY hprdId (hprdId(10)),
KEY uniProtId (uniProtId(10))
) TYPE=MyISAM;
