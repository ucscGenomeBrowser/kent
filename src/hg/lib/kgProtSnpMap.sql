CREATE TABLE kgProtSnpMap (
  snp  varchar(40) NOT NULL default '',		# ID of snp
  prot varchar(40) NOT NULL default '',		# protein ID
  chr  varchar(40)  NOT NULL default '',	# chromosome
  start int(10) unsigned NOT NULL default '0',  # start position
  end int(10) unsigned NOT NULL default '0',    # end postion
  strand char(1) NOT NULL default '',  		# strand
  iDNA int(10) unsigned NOT NULL default '0',   # iDNA
  snpAlleles  varchar(40)  NOT NULL default '',	# snpAlleles
  iCodon int(10) unsigned NOT NULL default '0', # iCodon
  iAA int(10) unsigned NOT NULL default '0',    # iAA
  codonAlleles char(2) NOT NULL default '',  	# codonAlleles 
  KEY snp (snp),
  KEY prot (prot)
) TYPE=MyISAM;

