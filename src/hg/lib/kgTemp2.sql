# used during KG build
CREATE TABLE kgTemp2 (
  name varchar(255) NOT NULL default '',
  chrom varchar(255) NOT NULL default '',
  strand char(1) NOT NULL default '',
  txStart int(10) unsigned NOT NULL default '0',
  txEnd int(10) unsigned NOT NULL default '0',
  cdsStart int(10) unsigned NOT NULL default '0',
  cdsEnd int(10) unsigned NOT NULL default '0',
  exonCount int(10) unsigned NOT NULL default '0',
  exonStarts longblob NOT NULL,
  exonEnds longblob NOT NULL,
  proteinID varchar(40) NOT NULL default '',
  alignID varchar(8) NOT NULL default '',
  KEY name (name),
  KEY protein (proteinID(12))
) TYPE=MyISAM;

