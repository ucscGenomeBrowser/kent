# This table contains duplicated mRNA/Protein entries which has identical cds structure
CREATE TABLE dupSpMrna (
  mrnaID varchar(20) NOT NULL default '',	# mRNA ID of main entry
  proteinID varchar(20) NOT NULL default '',	# protein ID of main entry
  dupMrnaID varchar(20) NOT NULL default '',	# mRNA ID of duplicated entry
  dupProteinID varchar(20) NOT NULL default '', # protein ID of duplicated entry
  KEY i3 (mrnaID(12)),
  KEY i4 (proteinID(12)),
  KEY i5 (dupMrnaID(12)),
  KEY i6 (dupProteinID(12))
) TYPE=MyISAM;

