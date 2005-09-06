CREATE TABLE geneNetworkId (
  Id varchar(40) NOT NULL default '',	# ID of GeneNetwork gene, currently RefSeqId
  KEY Id (Id(16))
) TYPE=MyISAM;

