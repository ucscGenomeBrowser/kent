-- MySQL dump 8.22
--
-- Host: localhost    Database: hgcentral
---------------------------------------------------------
-- Server version	3.23.54

--
-- Table structure for table 'sessionDb'
--

CREATE TABLE sessionDb (
  id int(10) unsigned NOT NULL auto_increment,
  contents longblob NOT NULL,
  reserved tinyint(4) NOT NULL default '0',
  firstUse datetime NOT NULL default '0000-00-00 00:00:00',
  lastUse datetime NOT NULL default '0000-00-00 00:00:00',
  useCount int(11) NOT NULL default '0',
  PRIMARY KEY  (id)
) TYPE=MyISAM PACK_KEYS=1;

--
-- Table structure for table 'userDb'
--

CREATE TABLE userDb (
  id int(10) unsigned NOT NULL auto_increment,
  contents longblob NOT NULL,
  reserved tinyint(4) NOT NULL default '0',
  firstUse datetime NOT NULL default '0000-00-00 00:00:00',
  lastUse datetime NOT NULL default '0000-00-00 00:00:00',
  useCount int(11) NOT NULL default '0',
  PRIMARY KEY  (id)
) TYPE=MyISAM PACK_KEYS=1;

-- MySQL dump 8.22
--
-- Host: localhost    Database: hgcentral
---------------------------------------------------------
-- Server version	3.23.54

--
-- Table structure for table 'defaultDb'
--

CREATE TABLE defaultDb (
  genome varchar(255) NOT NULL default '',
  name varchar(255) NOT NULL default '',
  PRIMARY KEY  (genome)
) TYPE=MyISAM;

--
-- Dumping data for table 'defaultDb'
--


INSERT INTO defaultDb (genome, name) VALUES ('C. briggsae','cb1');
INSERT INTO defaultDb (genome, name) VALUES ('Human','hg15');

--
-- Table structure for table 'blatServers'
--

CREATE TABLE blatServers (
  db varchar(32) NOT NULL default '',
  host varchar(128) NOT NULL default '',
  port int(11) NOT NULL default '0',
  isTrans tinyint(4) NOT NULL default '0',
  KEY db (db)
) TYPE=MyISAM PACK_KEYS=1;

--
-- Dumping data for table 'blatServers'
--


INSERT INTO blatServers (db, host, port, isTrans) VALUES ('cb1','blat10.cse.ucsc.edu',17782,0);
INSERT INTO blatServers (db, host, port, isTrans) VALUES ('cb1','blat10.cse.ucsc.edu',17783,1);

--
-- Table structure for table 'dbDb'
--

CREATE TABLE dbDb (
  name varchar(32) NOT NULL default '',
  description varchar(255) NOT NULL default '',
  nibPath varchar(255) NOT NULL default '',
  organism varchar(255) default NULL,
  defaultPos varchar(255) NOT NULL default '',
  active int(1) default '0',
  orderKey int(11) default '1000000',
  genome varchar(255) NOT NULL default '',
  scientificName varchar(255) default NULL,
  UNIQUE KEY name (name)
) TYPE=MyISAM PACK_KEYS=1;

--
-- Dumping data for table 'dbDb'
--


INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName) VALUES ('cb1','July 2002','/gbdb/cb1/nib','Worm','chrUn:74980670-74998831',1,10,'C. briggsae','Caenorhabditis briggsae');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName) VALUES ('hg15','April 2003','/gbdb/hg15/nib','Human','Select C. briggsae genome',1,70,'Human','Homo sapiens');

--
-- Table structure for table 'gdbPdb'
--

CREATE TABLE gdbPdb (
  genomeDb char(32) NOT NULL default '',
  proteomeDb char(32) NOT NULL default ''
) TYPE=MyISAM;

--
-- Dumping data for table 'gdbPdb'
--

# INSERT INTO gdbPdb (genomeDb, proteomeDb) VALUES ('hg15','proteins0405');

