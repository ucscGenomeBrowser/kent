#used udring KG build
CREATE TABLE geneCheck (
  acc varchar(40) NOT NULL default '',	# ID of KG entry
  chr varchar(255) NOT NULL default '',	# Chromosome name
  chrStart int(10) unsigned NOT NULL default '0',# Transcription start position
  chrEnd int(10) unsigned NOT NULL default '0',  # Transcription end position
  strand char(1) NOT NULL default '',		# Strand, + or -
  stat varchar(6),
  frame varchar(6),
  start varchar(6),
  stop varchar(6),
  
  orfStop int(6),
  cdsGap int(6),
  utrGap int(6),
  cdsSplice int(6),
  utrSplice int(6),

  numExons int(6), 
  numCds int(6), 
  numUtr5 int(6), 
  numUtr3 int(6), 
  numCdsIntrons int(6), 
  numUtrIntrons int(6), 
  nmd varchar(6),

  causes varchar(255),
  KEY acc(acc)
) TYPE=MyISAM;

