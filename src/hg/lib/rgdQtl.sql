CREATE TABLE rgdQtl (
  bin smallint(5) unsigned NOT NULL default '0',
  chrom varchar(255) NOT NULL default '',
  chromStart int(10) unsigned NOT NULL default '0',
  chromEnd int(10) unsigned NOT NULL default '0',
  name varchar(255) NOT NULL default '',
  KEY chrom (chrom(8),bin),
  KEY name (name(16)),
  KEY chrom_2 (chrom(8),chromStart),
  KEY chrom_3 (chrom(8),chromEnd)
) TYPE=MyISAM;

