CREATE TABLE inter (
  bin smallint(5) unsigned NOT NULL default '0',
  chrom varchar(255) NOT NULL default '',
  chromStart int(10) unsigned NOT NULL default '0',
  chromEnd int(10) unsigned NOT NULL default '0',
  name varchar(255) NOT NULL default '',
  KEY name (name(16)),
  KEY chrom (chrom(4),bin)
) TYPE=MyISAM;

