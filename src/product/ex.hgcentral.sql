-- MySQL dump 9.11
--
-- Host: localhost    Database: hgcentral
-- ------------------------------------------------------
-- Server version       4.0.21

--
-- Table structure for table 'blatServers'
--

CREATE TABLE blatServers (
  db char(32) NOT NULL default '',
  host char(128) NOT NULL default '',
  port int(11) NOT NULL default '0',
  isTrans tinyint(4) NOT NULL default '0',
  canPcr tinyint(4) NOT NULL default '0',
  KEY db (db)
) TYPE=MyISAM PACK_KEYS=1;

--
-- Dumping data for table 'blatServers'
--


INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('cb1','blat10',17782,0,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('cb1','blat10',17783,1,0);

--
-- Table structure for table `clade`
--

CREATE TABLE clade (
  name varchar(255) NOT NULL default '',
  label varchar(255) NOT NULL default '',
  priority float NOT NULL default '0',
  PRIMARY KEY  (name(16))
) TYPE=MyISAM;

--
-- Dumping data for table `clade`
--

INSERT INTO clade (name, label, priority) VALUES ('vertebrate','Vertebrate',10);
INSERT INTO clade (name, label, priority) VALUES ('deuterostome','Deuterostome',20);
INSERT INTO clade (name, label, priority) VALUES ('insect','Insect',30);
INSERT INTO clade (name, label, priority) VALUES ('worm','Nematode',40);
INSERT INTO clade (name, label, priority) VALUES ('other','Other',50);

--
-- Table structure for table 'dbDb'
--

CREATE TABLE dbDb (
  name varchar(32) NOT NULL default '',
  description varchar(255) NOT NULL default '',
  nibPath varchar(255) NOT NULL default '',
  organism varchar(255) NOT NULL default '',
  defaultPos varchar(255) NOT NULL default '',
  active int(1) NOT NULL default '0',
  orderKey int(11) NOT NULL default '1000000',
  genome varchar(255) NOT NULL default '',
  scientificName varchar(255) NOT NULL default '',
  htmlPath varchar(255) NOT NULL default '',
  hgNearOk tinyint(1) NOT NULL default '0',
  hgPbOk tinyint(4) NOT NULL default '0',
  sourceName varchar(255) NOT NULL default ''
) TYPE=MyISAM;

--
-- Dumping data for table 'dbDb'
--

INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('hg17','May 2004','/gbdb/hg17/nib','Human','chr7:127471196-127495720',0,10,'Human','Homo sapiens','/gbdb/hg17/html/description.html',1,1,'NCBI Build 35');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('cb1','July 2002','/gbdb/cb1/nib','C. briggsae','chrUn:74980670-74998831',1,70,'C. briggsae','Caenorhabditis briggsae','/gbdb/cb1/html/description.html',0,0,'WormBase v. cb25.agp8');

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

INSERT INTO defaultDb (genome, name) VALUES ('Human','hg17');
INSERT INTO defaultDb (genome, name) VALUES ('Mouse','mm5');
INSERT INTO defaultDb (genome, name) VALUES ('Rat','rn3');
INSERT INTO defaultDb (genome, name) VALUES ('Zoo','zooHuman3');
INSERT INTO defaultDb (genome, name) VALUES ('SARS','sc1');
INSERT INTO defaultDb (genome, name) VALUES ('C. elegans','ce2');
INSERT INTO defaultDb (genome, name) VALUES ('C. briggsae','cb1');
INSERT INTO defaultDb (genome, name) VALUES ('Fugu','fr1');
INSERT INTO defaultDb (genome, name) VALUES ('D. melanogaster','dm2');
INSERT INTO defaultDb (genome, name) VALUES ('D. pseudoobscura','dp2');
INSERT INTO defaultDb (genome, name) VALUES ('S. cerevisiae','sacCer1');
INSERT INTO defaultDb (genome, name) VALUES ('Chimp','panTro1');
INSERT INTO defaultDb (genome, name) VALUES ('Chicken','galGal2');
INSERT INTO defaultDb (genome, name) VALUES ('Dog','canFam1');
INSERT INTO defaultDb (genome, name) VALUES ('C. intestinalis','ci1');
INSERT INTO defaultDb (genome, name) VALUES ('D. yakuba','droYak1');
INSERT INTO defaultDb (genome, name) VALUES ('Zebrafish','danRer2');
INSERT INTO defaultDb (genome, name) VALUES ('A. gambiae','anoGam1');
INSERT INTO defaultDb (genome, name) VALUES ('Tetraodon','tetNig1');
INSERT INTO defaultDb (genome, name) VALUES ('X. tropicalis','xenTro1');
INSERT INTO defaultDb (genome, name) VALUES ('D. mojavensis','droMoj1');
INSERT INTO defaultDb (genome, name) VALUES ('A. mellifera','apiMel1');
INSERT INTO defaultDb (genome, name) VALUES ('D. virilis','droVir1');
INSERT INTO defaultDb (genome, name) VALUES ('D. ananassae','droAna1');
INSERT INTO defaultDb (genome, name) VALUES ('Opossum','monDom1');
INSERT INTO defaultDb (genome, name) VALUES ('Cow','bosTau1');

--
-- Table structure for table `gdbPdb`
--

CREATE TABLE gdbPdb (
  genomeDb char(32) NOT NULL default '',
  proteomeDb char(32) NOT NULL default ''
) TYPE=MyISAM;

--
-- Dumping data for table `gdbPdb`
--

INSERT INTO gdbPdb (genomeDb, proteomeDb) VALUES ('mm4','proteins040315');
INSERT INTO gdbPdb (genomeDb, proteomeDb) VALUES ('hg17','proteins041115');
INSERT INTO gdbPdb (genomeDb, proteomeDb) VALUES ('hg15','proteins040115');
INSERT INTO gdbPdb (genomeDb, proteomeDb) VALUES ('mm3','proteins092903');
INSERT INTO gdbPdb (genomeDb, proteomeDb) VALUES ('rn2','proteins040315');
INSERT INTO gdbPdb (genomeDb, proteomeDb) VALUES ('rn3','proteins040315');
INSERT INTO gdbPdb (genomeDb, proteomeDb) VALUES ('hg16','proteins040315');
INSERT INTO gdbPdb (genomeDb, proteomeDb) VALUES ('default','proteins040315');
INSERT INTO gdbPdb (genomeDb, proteomeDb) VALUES ('mm5','proteins040515');

--
-- Table structure for table `genomeClade`
--

CREATE TABLE genomeClade (
  genome varchar(255) NOT NULL default '',
  clade varchar(255) NOT NULL default '',
  priority float NOT NULL default '0',
  PRIMARY KEY  (genome(20))
) TYPE=MyISAM;

--
-- Dumping data for table `genomeClade`
--

INSERT INTO genomeClade (genome, clade, priority) VALUES ('Human','vertebrate',1);
INSERT INTO genomeClade (genome, clade, priority) VALUES ('Chimp','vertebrate',10);
INSERT INTO genomeClade (genome, clade, priority) VALUES ('Dog','vertebrate',20);
INSERT INTO genomeClade (genome, clade, priority) VALUES ('Mouse','vertebrate',40);
INSERT INTO genomeClade (genome, clade, priority) VALUES ('Rat','vertebrate',50);
INSERT INTO genomeClade (genome, clade, priority) VALUES ('Chicken','vertebrate',70);
INSERT INTO genomeClade (genome, clade, priority) VALUES ('X. tropicalis','vertebrate',80);
INSERT INTO genomeClade (genome, clade, priority) VALUES ('Zebrafish','vertebrate',90);
INSERT INTO genomeClade (genome, clade, priority) VALUES ('Tetraodon','vertebrate',100);
INSERT INTO genomeClade (genome, clade, priority) VALUES ('Fugu','vertebrate',110);
INSERT INTO genomeClade (genome, clade, priority) VALUES ('C. intestinalis','deuterostome',10);
INSERT INTO genomeClade (genome, clade, priority) VALUES ('D. melanogaster','insect',10);
INSERT INTO genomeClade (genome, clade, priority) VALUES ('D. yakuba','insect',30);
INSERT INTO genomeClade (genome, clade, priority) VALUES ('D. ananassae','insect',40);
INSERT INTO genomeClade (genome, clade, priority) VALUES ('D. pseudoobscura','insect',50);
INSERT INTO genomeClade (genome, clade, priority) VALUES ('D. virilis','insect',60);
INSERT INTO genomeClade (genome, clade, priority) VALUES ('D. mojavensis','insect',70);
INSERT INTO genomeClade (genome, clade, priority) VALUES ('A. mellifera','insect',80);
INSERT INTO genomeClade (genome, clade, priority) VALUES ('A. gambiae','insect',90);
INSERT INTO genomeClade (genome, clade, priority) VALUES ('C. elegans','worm',10);
INSERT INTO genomeClade (genome, clade, priority) VALUES ('C. briggsae','worm',20);
INSERT INTO genomeClade (genome, clade, priority) VALUES ('S. cerevisiae','other',10);
INSERT INTO genomeClade (genome, clade, priority) VALUES ('SARS','other',50);
INSERT INTO genomeClade (genome, clade, priority) VALUES ('Zoo','other',60);
INSERT INTO genomeClade (genome, clade, priority) VALUES ('Opossum','vertebrate',60);
INSERT INTO genomeClade (genome, clade, priority) VALUES ('Cow','vertebrate',30);

--
-- Table structure for table `liftOverChain`
--

CREATE TABLE liftOverChain (
  fromDb varchar(255) NOT NULL default '',
  toDb varchar(255) NOT NULL default '',
  path longblob NOT NULL
) TYPE=MyISAM;

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
