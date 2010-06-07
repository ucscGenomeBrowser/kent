# used during KG build
CREATE TABLE entrezRefseq (
  geneID varchar(40),
  refseq varchar(40) NOT NULL default '',
  KEY geneID(geneID)
) TYPE=MyISAM;

