# used during KG Build
CREATE TABLE entrezMrna (
  geneID varchar(40) NOT NULL default '',
  mrna varchar(40) NOT NULL default '',
  KEY geneID (geneID)
) TYPE=MyISAM;

