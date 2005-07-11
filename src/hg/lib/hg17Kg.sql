CREATE TABLE hg17Kg (
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
  KEY name (name(40)),
  KEY chrom (chrom(12),txStart),
  KEY chrom_2 (chrom(12),txEnd)
) TYPE=MyISAM;

