CREATE TABLE uniProtAlias (
  acc varchar(40) NOT NULL default '',  # UniProt accession number
  alias varchar(40) default NULL,	# alias, could be acc, old and new display IDs, etc.
  aliasSrc varchar(4) default NULL,	# source of this type of alias is from
  aliasSrcDate date default NULL,	# date of the source data
  KEY acc (acc)
) TYPE=MyISAM;

