
CREATE TABLE altGraphX (
  bin smallint unsigned not null,
  tName varchar(255) NOT NULL default '',
  tStart int(11) NOT NULL default '0',
  tEnd int(11) NOT NULL default '0',
  name varchar(255) NOT NULL default '',
  id int(10) unsigned NOT NULL auto_increment,
  strand char(2) NOT NULL default '',
  vertexCount int(10) unsigned NOT NULL default '0',
  vTypes longblob NOT NULL,
  vPositions longblob NOT NULL,
  edgeCount int(10) unsigned NOT NULL default '0',
  edgeStarts longblob NOT NULL,
  edgeEnds longblob NOT NULL,
  evidence longblob NOT NULL,
  edgeTypes longblob NOT NULL,
  mrnaRefCount int(11) NOT NULL default '0',
  mrnaRefs longblob NOT NULL,
  mrnaTissues longblob NOT NULL,
  mrnaLibs longblob NOT NULL,
  PRIMARY KEY  (id),
  KEY tName (tName(16),tStart,tEnd)
);

