# This table is the main table for the Known Genes (KG) track
CREATE TABLE kg2 (
  name varchar(255) NOT NULL default '',	# ID of KG entry
  chrom varchar(255) NOT NULL default '',	# Chromosome name
  strand char(1) NOT NULL default '',		# Strand, + or -
  txStart int(10) unsigned NOT NULL default '0',# Transcription start position
  txEnd int(10) unsigned NOT NULL default '0',  # Transcription end position
  cdsStart int(10) unsigned NOT NULL default '0',# Coding region start
  cdsEnd int(10) unsigned NOT NULL default '0',  # Coding region end
  exonCount int(10) unsigned NOT NULL default '0', # Number of exons
  exonStarts longblob NOT NULL,			# Exon start positions
  exonEnds longblob NOT NULL,			# Exon end positions
  proteinID varchar(40) NOT NULL default '',	# SWISS-PROT protein ID
  alignID varchar(8) NOT NULL default '',	# Unique identifier for each alignment	
  KEY name (name(16)),
  KEY chrom (chrom(16),txStart),
  KEY chrom_2 (chrom(16),txEnd),
  KEY protein (proteinID(16)),
  KEY align (alignID)
) TYPE=MyISAM;

