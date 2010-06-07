CREATE TABLE protHomolog (
  proteinID varchar(40) NOT NULL default '',	# protein ID
  homologID varchar(40) NOT NULL default '',	# homolog ID
  chain char(1) default NULL,			# homolog chain
  length int(11) default NULL,			# length
  bestEvalue double default NULL,		# best E value
  evalue double default NULL,			# E value
  FSSP varchar(40) default NULL,		# FSSP ID
  SCOPdomain varchar(40) default NULL,		# SCOP domain
  SCOPsuid varchar(40) default NULL,		# SCOP sunid
  KEY proteinID (proteinID),		
  KEY homologID (homologID)
) TYPE=MyISAM;

