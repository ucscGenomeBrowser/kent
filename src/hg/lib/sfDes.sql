CREATE TABLE sfDes (
  id int(12) NOT NULL default '0',
  level char(2) default NULL,
  classification text,
  name text,
  description text,
  KEY i1 (id)
) TYPE=MyISAM;

