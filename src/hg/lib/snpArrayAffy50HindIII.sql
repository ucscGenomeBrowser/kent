CREATE TABLE snpArrayAffy50HindIII (
  bin int(10) unsigned NOT NULL default '0',
  chrom varchar(255) NOT NULL default '',
  chromStart int(10) unsigned NOT NULL default '0',
  chromEnd int(10) unsigned NOT NULL default '0',
  name varchar(255) NOT NULL default '',
  score int(10) unsigned NOT NULL default '0',
  strand char(1) NOT NULL default '',
  observed blob NOT NULL,
  rsId varchar(64) default NULL
) TYPE=MyISAM;

