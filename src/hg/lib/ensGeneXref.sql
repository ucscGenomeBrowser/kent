CREATE TABLE ensGeneXref (
  db varchar(40) NOT NULL default '',
  analysis varchar(40) NOT NULL default '',
  type varchar(40) NOT NULL default '',
  gene_id int(10) unsigned NOT NULL default '0',
  gene_name varchar(40) NOT NULL default 'unknown',
  gene_version smallint(5) unsigned NOT NULL default '0',
  transcript_id int(10) unsigned NOT NULL default '0',
  transcript_name varchar(40) NOT NULL default '',
  transcript_version smallint(5) unsigned NOT NULL default '0',
  translation_name smallint(5) unsigned NOT NULL default '0',
  translation_id int(10) unsigned NOT NULL default '0',
  translation_version smallint(5) unsigned NOT NULL default '0',
  external_db varchar(40) NOT NULL default '',
  external_name varchar(40) NOT NULL default '',
  external_status varchar(10) NOT NULL default '',
  KEY gene_id (gene_id,transcript_id),
  KEY db (db,gene_id),
  KEY db_2 (db,external_db,external_name),
  KEY external_name (external_name),
  KEY tr1 (transcript_name)
) TYPE=MyISAM;

