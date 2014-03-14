# cross-reference table between the old and new Swiss-Prot/TrEMBL display IDs.
CREATE TABLE spOldNew (
  acc varchar(40) NOT NULL default '',		# primary accession number
  oldDisplayId varchar(40) NOT NULL default '', # old display ID
  newDisplayId varchar(40) NOT NULL default '', # new dipslay ID
  KEY acc(acc)
) ENGINE=MyISAM;

