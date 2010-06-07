# used during KG build
CREATE TABLE entrezRefProt (
  geneID varchar(40),
  refseq  varchar(40) NOT NULL default '',
  protAcc varchar(40) NOT NULL default '',
  KEY geneID(geneID)
) TYPE=MyISAM;

