CREATE TABLE dupSpMrna (
  mrnaID varchar(20) NOT NULL default '',
  proteinID varchar(20) NOT NULL default '',
  dupMrnaID varchar(20) NOT NULL default '',
  dupProteinID varchar(20) NOT NULL default '',
  KEY i3 (mrnaID(12)),
  KEY i4 (proteinID(12)),
  KEY i5 (dupMrnaID(12)),
  KEY i6 (dupProteinID(12))
) TYPE=MyISAM;

