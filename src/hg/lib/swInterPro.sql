/* cross-reference table between SWISS-PROT accession number and InterPro ID */
CREATE TABLE swInterPro (
  accession varchar(40) NOT NULL default '',
  interProId varchar(40) NOT NULL default '',
  KEY accession (accession),
  KEY interProId (interProId)
) TYPE=MyISAM;

