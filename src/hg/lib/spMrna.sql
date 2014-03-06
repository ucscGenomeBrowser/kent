# This table stores the protein/mrna pairs for the Known Genes track
CREATE TABLE spMrna (
  spID varchar(255) NOT NULL default '',	# SWISS-PROT protein ID
  mrnaID varchar(255) NOT NULL default '',	# mRNA accession number
  KEY  (spID)
) ENGINE=MyISAM;

