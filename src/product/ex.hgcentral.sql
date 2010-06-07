-- This file last updated: $Date: 2006/08/12 22:15:43 $
--
-- MySQL dump 9.11
--
-- Host: localhost    Database: hgcentral
-- ------------------------------------------------------
-- Server version	4.0.27

--
-- Table structure for table `sessionDb`
--

CREATE TABLE sessionDb (
  id int(10) unsigned NOT NULL auto_increment,
  contents longblob NOT NULL,
  reserved tinyint(4) NOT NULL default '0',
  firstUse datetime NOT NULL default '0000-00-00 00:00:00',
  lastUse datetime NOT NULL default '0000-00-00 00:00:00',
  useCount int(11) NOT NULL default '0',
  PRIMARY KEY  (id)
) TYPE=MyISAM;

--
-- Table structure for table `userDb`
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

-- MySQL dump 9.11
--
-- Host: localhost    Database: hgcentral
-- ------------------------------------------------------
-- Server version	4.0.27

--
-- Table structure for table `defaultDb`
--

CREATE TABLE defaultDb (
  genome varchar(255) NOT NULL default '',
  name varchar(255) NOT NULL default '',
  PRIMARY KEY  (genome)
) TYPE=MyISAM;

--
-- Dumping data for table `defaultDb`
--

INSERT INTO defaultDb (genome, name) VALUES ('Human','hg18');
INSERT INTO defaultDb (genome, name) VALUES ('Mouse','mm8');
INSERT INTO defaultDb (genome, name) VALUES ('Rat','rn4');
INSERT INTO defaultDb (genome, name) VALUES ('Zoo','zooHuman3');
INSERT INTO defaultDb (genome, name) VALUES ('SARS','sc1');
INSERT INTO defaultDb (genome, name) VALUES ('C. elegans','ce2');
INSERT INTO defaultDb (genome, name) VALUES ('C. briggsae','cb1');
INSERT INTO defaultDb (genome, name) VALUES ('Fugu','fr1');
INSERT INTO defaultDb (genome, name) VALUES ('D. melanogaster','dm2');
INSERT INTO defaultDb (genome, name) VALUES ('D. pseudoobscura','dp3');
INSERT INTO defaultDb (genome, name) VALUES ('S. cerevisiae','sacCer1');
INSERT INTO defaultDb (genome, name) VALUES ('Chimp','panTro2');
INSERT INTO defaultDb (genome, name) VALUES ('Chicken','galGal2');
INSERT INTO defaultDb (genome, name) VALUES ('Dog','canFam2');
INSERT INTO defaultDb (genome, name) VALUES ('C. intestinalis','ci2');
INSERT INTO defaultDb (genome, name) VALUES ('D. yakuba','droYak2');
INSERT INTO defaultDb (genome, name) VALUES ('Zebrafish','danRer4');
INSERT INTO defaultDb (genome, name) VALUES ('A. gambiae','anoGam1');
INSERT INTO defaultDb (genome, name) VALUES ('Tetraodon','tetNig1');
INSERT INTO defaultDb (genome, name) VALUES ('X. tropicalis','xenTro2');
INSERT INTO defaultDb (genome, name) VALUES ('D. mojavensis','droMoj2');
INSERT INTO defaultDb (genome, name) VALUES ('A. mellifera','apiMel2');
INSERT INTO defaultDb (genome, name) VALUES ('D. virilis','droVir2');
INSERT INTO defaultDb (genome, name) VALUES ('D. ananassae','droAna2');
INSERT INTO defaultDb (genome, name) VALUES ('Opossum','monDom1');
INSERT INTO defaultDb (genome, name) VALUES ('Cow','bosTau2');
INSERT INTO defaultDb (genome, name) VALUES ('Rhesus','rheMac2');
INSERT INTO defaultDb (genome, name) VALUES ('D. simulans','droSim1');
INSERT INTO defaultDb (genome, name) VALUES ('D. erecta','droEre1');
INSERT INTO defaultDb (genome, name) VALUES ('D. grimshawi','droGri1');
INSERT INTO defaultDb (genome, name) VALUES ('D. persimilis','droPer1');
INSERT INTO defaultDb (genome, name) VALUES ('D. sechellia','droSec1');
INSERT INTO defaultDb (genome, name) VALUES ('S. purpuratus','strPur1');

--
-- Table structure for table `blatServers`
--

CREATE TABLE blatServers (
  db varchar(32) NOT NULL default '',
  host varchar(128) NOT NULL default '',
  port int(11) NOT NULL default '0',
  isTrans tinyint(4) NOT NULL default '0',
  canPcr tinyint(4) NOT NULL default '0',
  KEY db (db)
) TYPE=MyISAM PACK_KEYS=1;

--
-- Dumping data for table `blatServers`
--

INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('galGal2','blat8',17780,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('sc1','blat11',17780,0,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('sc1','blat11',17781,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('hg15','blat15',17790,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('panTro1','blat4',17780,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('panTro1','blat4',17781,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('anoGam1','blat8',17779,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('mm6','blat15',17779,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('mm6','blat15',17778,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('mm8','blat2',17785,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('mm8','blat2',17784,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('ce1','blat8',17788,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('ce1','blat8',17789,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('cb1','blat8',17787,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('cb1','blat8',17786,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('rn3','blat11',17778,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('rn3','blat11',17779,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('dp3','blat8',17798,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('dp3','blat8',17799,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('droSim1','blat4',17786,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('droSim1','blat4',17787,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('hg15','blat15',17791,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('danRer4','blat2',17789,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('apiMel2','blat14',17792,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('apiMel2','blat14',17793,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('bosTau1','blat13',17779,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('bosTau1','blat13',17778,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('dm2','blat14',17795,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('dm2','blat14',17794,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('monDom1','blat14',17787,0,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('monDom1','blat14',17786,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('hg16','blat13',17784,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('hg16','blat13',17785,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('fr1','blat11',17783,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('fr1','blat11',17782,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('dm1','blat8',17794,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('dm1','blat8',17795,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('danRer4','blat2',17788,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('sacCer1','blat8',17800,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('sacCer1','blat8',17801,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('galGal2','blat8',17781,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('ce2','blat8',17790,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('ce2','blat8',17791,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('tetNig1','blat8',17782,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('dp2','blat8',17797,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('ci1','blat8',17792,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('ci1','blat8',17793,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('mm5','blat13',17783,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('mm5','blat13',17782,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('hg17','blat12',17779,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('hg17','blat12',17778,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('droYak1','blat4',17788,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('droYak1','blat4',17789,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('rn2','blat3',17785,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('rn2','blat3',17784,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('anoGam1','blat8',17778,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('dp2','blat8',17796,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('canFam1','blat4',17779,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('canFam1','blat4',17778,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('tetNig1','blat8',17783,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('xenTro1','blat13',17781,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('xenTro1','blat13',17780,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('droMoj1','blat14',17782,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('droMoj1','blat14',17783,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('droVir1','blat14',17785,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('droVir1','blat14',17784,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('droAna1','blat14',17780,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('droAna1','blat14',17781,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('apiMel1','blat14',17778,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('apiMel1','blat14',17779,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('danRer2','blat14',17788,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('danRer2','blat14',17789,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('ci2','blat4',17784,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('ci2','blat4',17785,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('droVir2','blat6',17785,0,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('droVir2','blat6',17784,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('droMoj2','blat6',17787,0,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('droMoj2','blat6',17786,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('droEre1','blat9',17787,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('droEre1','blat9',17786,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('bosTau2','blat9',17782,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('bosTau2','blat9',17783,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('droAna2','blat9',17784,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('droAna2','blat9',17785,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('droGri1','blat6',17782,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('droGri1','blat6',17783,0,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('mm7','blat9',17778,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('mm7','blat9',17779,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('canFam2','blat3',17783,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('canFam2','blat3',17782,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('droPer1','blat6',17790,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('droPer1','blat6',17791,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('droYak2','blat2',17780,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('droYak2','blat2',17781,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('droSec1','blat2',17782,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('droSec1','blat2',17783,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('rheMac2','blat8',17802,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('rheMac2','blat8',17803,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('danRer3','blat3',17778,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('danRer3','blat3',17779,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('strPur1','blat4',17790,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('strPur1','blat4',17791,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('hg18','blat3',17780,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('hg18','blat3',17781,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('xenTro2','blat4',17783,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('xenTro2','blat4',17782,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('rn4','blat6',17792,1,0);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('rn4','blat6',17793,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('panTro2','blat12',17791,0,1);
INSERT INTO blatServers (db, host, port, isTrans, canPcr) VALUES ('panTro2','blat12',17790,1,0);

--
-- Table structure for table `dbDb`
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
  hgNearOk tinyint(4) NOT NULL default '0',
  hgPbOk tinyint(4) NOT NULL default '0',
  sourceName varchar(255) NOT NULL default ''
) TYPE=MyISAM;

--
-- Dumping data for table `dbDb`
--

INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('dm1','Jan. 2003','/gbdb/dm1/nib','D. melanogaster','chr2L:827700-845800',1,52,'D. melanogaster','Drosophila melanogaster','/gbdb/dm1/html/description.html',1,0,'BDGP v. 3');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('sc1','Apr. 2003','/gbdb/sc1/nib','SARS','chr1',1,100,'SARS','SARS coronavirus','/gbdb/sc1/html/description.html',0,0,'GenBank sequence as of 14 Apr 2003');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('rn2','Jan. 2003','/gbdb/rn2/nib','Rat','chr5:186572878-186580717',1,31,'Rat','Rattus norvegicus','/gbdb/rn2/html/description.html',1,0,'Baylor HGSC v. 2.1');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('sacCer1','Oct. 2003','/gbdb/sacCer1/nib','S. cerevisiae','chr13:746300-756100',1,80,'S. cerevisiae','Saccharomyces cerevisiae','/gbdb/sacCer1/html/description.html',1,0,'SGD sequence data as of 1 Oct. 2003');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('dp2','Aug. 2003','/gbdb/dp2/nib','D. pseudoobscura','Contig1006_Contig943:3363975-3378814',1,56,'D. pseudoobscura','Drosophila pseudoobscura','/gbdb/dp2/html/description.html',0,0,'Baylor HGSC Freeze 1');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('hg15','Apr. 2003','/gbdb/hg15/nib','Human','chr7:26828631-26938371',1,12,'Human','Homo sapiens','/gbdb/hg15/html/description.html',1,0,'NCBI Build 33');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('ce1','May 2003','/gbdb/ce1/nib','C. elegans','chrII:14642289-14671631',1,61,'C. elegans','Caenorhabditis elegans','/gbdb/ce1/html/description.html',1,0,'WormBase v. WS100');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('cb1','July 2002','/gbdb/cb1/nib','C. briggsae','chrUn:74980670-74998831',1,70,'C. briggsae','Caenorhabditis briggsae','/gbdb/cb1/html/description.html',0,0,'WormBase v. cb25.agp8');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('hg16','July 2003','/gbdb/hg16/nib','Human','chr4:56214201-56291736',1,11,'Human','Homo sapiens','/gbdb/hg16/html/description.html',1,1,'NCBI Build 34');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('rn3','June 2003','/gbdb/rn3/nib','Rat','chr5:136311757-136338435',1,30,'Rat','Rattus norvegicus','/gbdb/rn3/html/description.html',1,1,'Baylor HGSC v. 3.1');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('danRer4','Mar. 2006','/gbdb/danRer4','Zebrafish','chr13:16,747,248-16,763,118',1,37,'Zebrafish','Danio rerio','/gbdb/danRer4/html/description.html',0,0,'Sanger Centre, Danio rerio Sequencing Project Zv6');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('xenTro2','Aug. 2005','/gbdb/xenTro2','X. tropicalis','scaffold_27:565,941-578,374',1,36,'X. tropicalis','Xenopus tropicalis','/gbdb/xenTro2/html/description.html',0,0,'JGI v4.1');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('panTro2','Mar 2006','/gbdb/panTro2','Chimp','chr6:12177375-12405661',1,14,'Chimp','Pan troglodytes','/gbdb/panTro2/html/description.html',0,0,'Washington University Build 2 Version 1');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('mm8','Feb 2006','/gbdb/mm8','Mouse','chr12:50258170-50263946',1,22,'Mouse','Mus musculus','/gbdb/mm8/html/description.html',1,1,'NCBI Build 36');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('monDom4','Jan 2006','/gbdb/monDom4','Opossum','chr4:344283621-363415496',0,34,'Opossum','Monodelphis domestica','/gbdb/monDom4/html/description.html',0,0,'Broad Inst. monDom4 Jan06');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('rn4','Nov. 2004','/gbdb/rn4','Rat','chr1:79,003,435-79,006,520',1,29,'Rat','Rattus norvegicus','/gbdb/rn4/html/description.html',1,1,'Baylor HGSC v. 3.4');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('strPur1','April 2005','/gbdb/strPur1','S. purpuratus','Scaffold1',1,80,'S. purpuratus','Strongylocentrotus purpuratus','/gbdb/strPur1/html/description.html',0,0,'Baylor');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('fr1','Aug. 2002','/gbdb/fr1/nib','Fugu','chrUn:148538552-148543911',1,40,'Fugu','Takifugu rubripes','/gbdb/fr1/html/description.html',0,0,'JGI V3.0');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('panTro1','Nov. 2003','/gbdb/panTro1/nib','Chimp','chr6:115705331-115981791',1,15,'Chimp','Pan troglodytes','/gbdb/panTro1/html/description.html',0,0,'CGSC Build 1 Version 1 Arachne assembly');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('galGal2','Feb. 2004','/gbdb/galGal2/nib','Chicken','chr13:13120186-13124117',1,35,'Chicken','Gallus gallus','/gbdb/galGal2/html/description.html',0,0,'Chicken Genome Sequencing Consortium Feb. 2004 release');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('ce2','Mar. 2004','/gbdb/ce2/nib','C. elegans','chrII:14642289-14671631',1,60,'C. elegans','Caenorhabditis elegans','/gbdb/ce2/html/description.html',1,0,'WormBase v. WS120');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('rheMac1','Jan. 2005','/gbdb/rheMac1','Rhesus','SCAFFOLD125093:168700-200800',1,16,'Rhesus','Macaca mulatta','/gbdb/rheMac1/html/description.html',0,0,'Baylor v0.1');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('mm6','Mar. 2005','/gbdb/mm6','Mouse','chrX:87947304-87959012',1,24,'Mouse','Mus musculus','/gbdb/mm6/html/description.html',1,1,'NCBI Build 34');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('xenTro1','Oct. 2004','/gbdb/xenTro1','X. tropicalis','scaffold_12:3462738-3490440',1,36,'X. tropicalis','Xenopus tropicalis','/gbdb/xenTro1/html/description.html',0,0,'JGI v3.0');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('canFam1','July 2004','/gbdb/canFam1/nib','Dog','chr14:10666612-10673232',1,18,'Dog','Canis familiaris','/gbdb/canFam1/html/description.html',0,0,'Broad Institute v. 1.0');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('ci1','Dec. 2002','/gbdb/ci1/nib','C. intestinalis','Scaffold_490:1-51949',1,45,'C. intestinalis','Ciona intestinalis','/gbdb/ci1/html/description.html',0,0,'JGI v1.0');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('mm5','May 2004','/gbdb/mm5/nib','Mouse','chr6:28912411-28925620',1,25,'Mouse','Mus musculus','/gbdb/mm5/html/description.html',1,1,'NCBI Build 33');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('hg17','May 2004','/gbdb/hg17/nib','Human','chr7:127471196-127495720',1,10,'Human','Homo sapiens','/gbdb/hg17/html/description.html',1,1,'NCBI Build 35');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('droYak1','Apr. 2004','/gbdb/droYak1/nib','D. yakuba','chr2L:827700-845800',1,53,'D. yakuba','Drosophila yakuba','/gbdb/droYak1/html/description.html',0,0,'WUSTL version 1.0');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('danRer1','Nov. 2003','/gbdb/danRer1/nib','Zebrafish','chr2:16330443-16335196',0,38,'Zebrafish','Danio rerio','/gbdb/danRer1/html/description.html',0,0,'Sanger Centre, Danio rerio Sequencing Project Zv3');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('anoGam1','Feb. 2003','/gbdb/anoGam1/nib','A. gambiae','chr2R:6479796-6482329',1,59,'A. gambiae','Anopheles gambiae','/gbdb/anoGam1/html/description.html',0,0,'IAGEC v.MOZ2');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('tetNig1','Feb. 2004','/gbdb/tetNig1/nib','Tetraodon','chr10:2344000-2351500',1,39,'Tetraodon','Tetraodon nigroviridis','/gbdb/tetNig1/html/description.html',0,0,'Genoscope and Broad Institute, V7');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('droMoj1','Aug. 2004','/gbdb/droMoj1','D. mojavensis','contig_2959:30001-44626',1,57,'D. mojavensis','Drosophila mojavensis','/gbdb/droMoj1/html/description.html',0,0,'Agencourt 11 Aug 2004');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('apiMel2','Jan. 2005','/gbdb/apiMel2','A. mellifera','GroupUn:47426601-47431800',1,57,'A. mellifera','Apis mellifera','/gbdb/apiMel2/html/description.html',0,0,'Baylor HGSC Amel_2.0');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('droVir1','July 2004','/gbdb/droVir1','D. virilis','scaffold_6:5128001-5158000',1,57,'D. virilis','Drosophila virilis','/gbdb/droVir1/html/description.html',0,0,'Agencourt 12 July 2004');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('droAna1','July 2004','/gbdb/droAna1','D. ananassae','2448876:2700001-2725000',1,57,'D. ananassae','Drosophila ananassae','/gbdb/droAna1/html/description.html',0,0,'TIGR 15 July 2004');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('apiMel1','July 2004','/gbdb/apiMel1','A. mellifera','GroupUn.5651:1001-6000',1,58,'A. mellifera','Apis mellifera','/gbdb/apiMel1/html/description.html',0,0,'Baylor HGSC Amel_1.2');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('danRer2','June 2004','/gbdb/danRer2','Zebrafish','chr2:15,906,734-15,926,406',1,37,'Zebrafish','Danio rerio','/gbdb/danRer2/html/description.html',0,0,'Sanger Centre, Danio rerio Sequencing Project Zv4');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('monDom1','Oct. 2004','/gbdb/monDom1','Opossum','scaffold_18580:4449881-4565227',1,34,'Opossum','Monodelphis domestica','/gbdb/monDom1/html/description.html',0,0,'Broad Inst. Prelim Oct04');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('dm2','Apr. 2004','/gbdb/dm2/nib','D. melanogaster','chr2L:825964-851061',1,51,'D. melanogaster','Drosophila melanogaster','/gbdb/dm2/html/description.html',1,0,'BDGP v. 4 / DHGP v. 3.2');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('bosTau1','Sep. 2004','/gbdb/bosTau1','Cow','SCAFFOLD100016:1-30000',1,19,'Cow','Bos taurus','/gbdb/bosTau1/html/description.html',0,0,'Baylor v1.0');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('dp3','Nov. 2004','/gbdb/dp3','D. pseudoobscura','chr4_group3:5603001-5614000',1,56,'D. pseudoobscura','Drosophila pseudoobscura','/gbdb/dp3/html/description.html',0,0,'FlyBase Release 1.03');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('droSim1','Apr. 2005','/gbdb/droSim1','D. simulans','chr2L:827,150-853,526',1,52,'D. simulans','Drosophila simulans','/gbdb/droSim1/html/description.html',0,0,'WUSTL version 1.0');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('mm3','Feb. 2003','/gbdb/mm3/nib','Mouse','chr6:121947466-121963394',0,27,'Mouse','Mus musculus','/gbdb/mm3/html/description.html',0,0,'NCBI Build 30');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('ci2','Mar. 2005','/gbdb/ci2/nib','C. intestinalis','chr01p:3,108,502-3,170,702',1,44,'C. intestinalis','Ciona intestinalis','/gbdb/ci2/html/description.html',0,0,'JGI v2.0');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('droVir2','Aug. 2005','/gbdb/droVir2','D. virilis','scaffold_12963:19375824-19416317',1,57,'D. virilis','Drosophila virilis','/gbdb/droVir2/html/description.html',0,0,'Agencourt 1 August 2005');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('droMoj2','Aug. 2005','/gbdb/droMoj2','D. mojavensis','scaffold_6500:23838548-23875233',1,57,'D. mojavensis','Drosophila mojavensis','/gbdb/droMoj2/html/description.html',0,0,'Agencourt 1 August 2005');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('droEre1','Aug. 2005','/gbdb/droEre1','D. erecta','scaffold_4929:882464-907886',1,53,'D. erecta','Drosophila erecta','/gbdb/droEre1/html/description.html',0,0,'Agencourt 1 Aug 2005');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('bosTau2','Mar. 2005','/gbdb/bosTau2','Cow','SCAFFOLD10671:4650-8779',1,19,'Cow','Bos taurus','/gbdb/bosTau2/html/description.html',0,0,'Baylor Btau_2.0');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('droAna2','Aug. 2005','/gbdb/droAna2','D. ananassae','scaffold_12916:2717000-2741384',1,54,'D. ananassae','Drosophila ananassae','/gbdb/droAna2/html/description.html',0,0,'Agencourt 1 August 2005');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('droGri1','Aug. 2005','/gbdb/droGri1','D. grimshawi','scaffold_24862:854980-869643',1,57,'D. grimshawi','Drosophila grimshawi','/gbdb/droGri1/html/description.html',0,0,'Agencourt 1 Aug 2005');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('mm7','Aug. 2005','/gbdb/mm7','Mouse','chr12:47396009-47402736',1,23,'Mouse','Mus musculus','/gbdb/mm7/html/description.html',1,1,'NCBI Build 35');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('canFam2','May 2005','/gbdb/canFam2','Dog','chr14:11072309-11078928',1,18,'Dog','Canis familiaris','/gbdb/canFam2/html/description.html',0,0,'Broad Institute v. 2.0');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('dasNov1','May. 2005','/gbdb/dasNov1','Armadillo','scaffold_4294:1-90,852',0,32,'Armadillo','Dasypus novemcinctus','/gbdb/dasNov1/html/description.html',0,0,'Broad May 2005');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('loxAfr1','May. 2005','/gbdb/loxAfr1','Elephant','scaffold_197:6,946-230,632',0,33,'Elephant','Loxodonta africana','/gbdb/loxAfr1/html/description.html',0,0,'Broad May 2005');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('monDom2','June 2005','/gbdb/monDom2','Opossum','scaffold_0:1000000-11000000',0,34,'Opossum','Monodelphis domestica','/gbdb/monDom2/html/description.html',0,0,'Broad Inst. V3 Prelim Jun05');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('danRer3','May 2005','/gbdb/danRer3','Zebrafish','chr13:15,409,989-15,425,511',1,37,'Zebrafish','Danio rerio','/gbdb/danRer3/html/description.html',0,0,'Sanger Centre, Danio rerio Sequencing Project Zv5');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('oryCun1','May. 2005','/gbdb/oryCun1','Rabbit','scaffold_203931:16852-20386',0,32,'Rabbit','Oryctolagus cuniculus','/gbdb/oryCun1/html/description.html',0,0,'Broad May 2005');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('echTel1','July. 2005','/gbdb/echTel1','Tenrec','scaffold_287419:9,981-66,345',0,33,'Tenrec','Echinops telfairi','/gbdb/echTel1/html/description.html',0,0,'Broad July 2005');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('droPer1','Oct. 2005','/gbdb/droPer1','D. persimilis','super_1:7109001-7125000',1,56,'D. persimilis','Drosophila persimilis','/gbdb/droPer1/html/description.html',0,0,'Broad 28 Oct. 2005');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('droYak2','Nov. 2005','/gbdb/droYak2','D. yakuba','chr2L:809001-825000',1,53,'D. yakuba','Drosophila yakuba','/gbdb/droYak2/html/description.html',0,0,'WUSTL version 2.0');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('droSec1','Oct. 2005','/gbdb/droSec1','D. sechellia','super_14:795001-815000',1,52,'D. sechellia','Drosophila sechellia','/gbdb/droSec1/html/description.html',0,0,'Broad 28 Oct. 2005');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('rheMac2','Jan. 2006','/gbdb/rheMac2','Rhesus','chr7:58999504-59100350',1,16,'Rhesus','Macaca mulatta','/gbdb/rheMac2/html/description.html',0,0,'Venter Inst./Baylor/WashU Merged assembly Jan 2006 v1 edit4');
INSERT INTO dbDb (name, description, nibPath, organism, defaultPos, active, orderKey, genome, scientificName, htmlPath, hgNearOk, hgPbOk, sourceName) VALUES ('hg18','Mar. 2006','/gbdb/hg18/nib','Human','chrX:151073054-151383976',1,9,'Human','Homo sapiens','/gbdb/hg18/html/description.html',1,1,'NCBI Build 36.1');

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

INSERT INTO gdbPdb (genomeDb, proteomeDb) VALUES ('hg17','proteins050415');
INSERT INTO gdbPdb (genomeDb, proteomeDb) VALUES ('hg15','proteins040115');
INSERT INTO gdbPdb (genomeDb, proteomeDb) VALUES ('mm6','proteins050415');
INSERT INTO gdbPdb (genomeDb, proteomeDb) VALUES ('rn2','proteins040315');
INSERT INTO gdbPdb (genomeDb, proteomeDb) VALUES ('rn3','proteins040315');
INSERT INTO gdbPdb (genomeDb, proteomeDb) VALUES ('hg16','proteins040315');
INSERT INTO gdbPdb (genomeDb, proteomeDb) VALUES ('default','proteins040315');
INSERT INTO gdbPdb (genomeDb, proteomeDb) VALUES ('mm5','proteins040515');
INSERT INTO gdbPdb (genomeDb, proteomeDb) VALUES ('mm7','proteins051015');
INSERT INTO gdbPdb (genomeDb, proteomeDb) VALUES ('hg18','proteins060115');
INSERT INTO gdbPdb (genomeDb, proteomeDb) VALUES ('mm8','proteins060115');
INSERT INTO gdbPdb (genomeDb, proteomeDb) VALUES ('rn4','proteins060115');

--
-- Table structure for table `liftOverChain`
--

CREATE TABLE liftOverChain (
  fromDb varchar(255) NOT NULL default '',
  toDb varchar(255) NOT NULL default '',
  path longblob NOT NULL,
  minMatch float NOT NULL default '0',
  minSizeT int(10) unsigned NOT NULL default '0',
  minSizeQ int(10) unsigned NOT NULL default '0',
  multiple char(1) NOT NULL default '',
  minBlocks float NOT NULL default '0',
  fudgeThick char(1) NOT NULL default ''
) TYPE=MyISAM;

--
-- Dumping data for table `liftOverChain`
--

INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg13','hg15','/gbdb/hg13/liftOver/hg13ToHg15.over.chain.gz',0.95,0,0,'N',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg13','hg16','/gbdb/hg13/liftOver/hg13ToHg16.over.chain.gz',0.95,0,0,'N',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg15','hg16','/gbdb/hg15/liftOver/hg15ToHg16.over.chain.gz',0.95,0,0,'N',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg12','hg15','/gbdb/hg12/liftOver/hg12ToHg15.over.chain.gz',0.95,0,0,'N',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg12','hg16','/gbdb/hg12/liftOver/hg12ToHg16.over.chain.gz',0.95,0,0,'N',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg12','hg13','/gbdb/hg12/liftOver/hg12ToHg13.over.chain.gz',0.95,0,0,'N',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg10','hg16','/gbdb/hg10/liftOver/hg10ToHg16.over.chain.gz',0.95,0,0,'N',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg16','hg17','/gbdb/hg16/liftOver/hg16ToHg17.over.chain.gz',0.95,0,0,'N',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg17','mm5','/gbdb/hg17/liftOver/hg17ToMm5.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('dm1','dm2','/gbdb/dm1/liftOver/dm1ToDm2.over.chain.gz',0.95,0,0,'N',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm5','canFam1','/gbdb/mm5/liftOver/mm5ToCanFam1.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg17','hg16','/gbdb/hg17/liftOver/hg17ToHg16.over.chain.gz',0.95,0,0,'N',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('rn3','hg17','/gbdb/rn3/liftOver/rn3ToHg17.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg15','hg17','/gbdb/hg15/liftOver/hg15ToHg17.over.chain.gz',0.95,0,0,'N',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm5','bosTau1','/gbdb/mm5/liftOver/mm5ToBosTau1.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('rn3','mm5','/gbdb/rn3/liftOver/rn3ToMm5.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm4','mm7','/gbdb/mm4/liftOver/mm4ToMm7.over.chain.gz',0.95,0,0,'N',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg17','galGal2','/gbdb/hg17/liftOver/hg17ToGalGal2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm4','mm5','/gbdb/mm4/liftOver/mm4ToMm5.over.chain.gz',0.95,0,0,'N',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('canFam2','mm6','/gbdb/canFam2/liftOver/canFam2ToMm6.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('canFam2','mm7','/gbdb/canFam2/liftOver/canFam2ToMm7.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm3','mm7','/gbdb/mm3/liftOver/mm3ToMm7.over.chain.gz',0.95,0,0,'N',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg17','danRer2','/gbdb/hg17/liftOver/hg17ToDanRer2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg17','canFam2','/gbdb/hg17/liftOver/hg17ToCanFam2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg17','canFam1','/gbdb/hg17/liftOver/hg17ToCanFam1.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('rheMac1','hg17','/gbdb/rheMac1/liftOver/rheMac1ToHg17.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('bosTau1','hg17','/gbdb/bosTau1/liftOver/bosTau1ToHg17.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('bosTau1','mm6','/gbdb/bosTau1/liftOver/bosTau1ToMm6.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg17','bosTau2','/gbdb/hg17/liftOver/hg17ToBosTau2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm2','mm5','/gbdb/mm2/liftOver/mm2ToMm5.over.chain.gz',0.95,0,0,'N',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm3','mm6','/gbdb/mm3/liftOver/mm3ToMm6.over.chain.gz',0.95,0,0,'N',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('bosTau1','mm5','/gbdb/bosTau1/liftOver/bosTau1ToMm5.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg17','bosTau1','/gbdb/hg17/liftOver/hg17ToBosTau1.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('canFam2','hg17','/gbdb/canFam2/liftOver/canFam2ToHg17.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('bosTau1','bosTau2','/gbdb/bosTau1/liftOver/bosTau1ToBosTau2.over.chain.gz',0.95,0,0,'N',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('canFam2','rn3','/gbdb/canFam2/liftOver/canFam2ToRn3.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('canFam1','mm6','/gbdb/canFam1/liftOver/canFam1ToMm6.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm5','galGal2','/gbdb/mm5/liftOver/mm5ToGalGal2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm5','hg17','/gbdb/mm5/liftOver/mm5ToHg17.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm5','mm6','/gbdb/mm5/liftOver/mm5ToMm6.over.chain.gz',0.95,0,0,'N',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm5','mm7','/gbdb/mm5/liftOver/mm5ToMm7.over.chain.gz',0.95,0,0,'N',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm5','rn3','/gbdb/mm5/liftOver/mm5ToRn3.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm6','bosTau1','/gbdb/mm6/liftOver/mm6ToBosTau1.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm6','canFam1','/gbdb/mm6/liftOver/mm6ToCanFam1.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm6','canFam2','/gbdb/mm6/liftOver/mm6ToCanFam2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm6','danRer2','/gbdb/mm6/liftOver/mm6ToDanRer2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('droGri1','dm2','/gbdb/droGri1/liftOver/droGri1ToDm2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm6','fr1','/gbdb/mm6/liftOver/mm6ToFr1.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm6','galGal2','/gbdb/mm6/liftOver/mm6ToGalGal2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm6','hg16','/gbdb/mm6/liftOver/mm6ToHg16.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm6','hg17','/gbdb/mm6/liftOver/mm6ToHg17.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm6','mm7','/gbdb/mm6/liftOver/mm6ToMm7.over.chain.gz',0.95,0,0,'N',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm6','monDom1','/gbdb/mm6/liftOver/mm6ToMonDom1.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm6','panTro1','/gbdb/mm6/liftOver/mm6ToPanTro1.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm6','rn3','/gbdb/mm6/liftOver/mm6ToRn3.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm6','tetNig1','/gbdb/mm6/liftOver/mm6ToTetNig1.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm6','xenTro1','/gbdb/mm6/liftOver/mm6ToXenTro1.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('rn3','mm6','/gbdb/rn3/liftOver/rn3ToMm6.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('rn3','canFam2','/gbdb/rn3/liftOver/rn3ToCanFam2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('canFam1','hg17','/gbdb/canFam1/liftOver/canFam1ToHg17.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('canFam1','mm5','/gbdb/canFam1/liftOver/canFam1ToMm5.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg17','mm6','/gbdb/hg17/liftOver/hg17ToMm6.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg17','monDom1','/gbdb/hg17/liftOver/hg17ToMonDom1.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg17','panTro1','/gbdb/hg17/liftOver/hg17ToPanTro1.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg17','rheMac2','/gbdb/hg17/liftOver/hg17ToRheMac2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg17','rn3','/gbdb/hg17/liftOver/hg17ToRn3.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg17','tetNig1','/gbdb/hg17/liftOver/hg17ToTetNig1.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg17','xenTro1','/gbdb/hg17/liftOver/hg17ToXenTro1.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm7','canFam2','/gbdb/mm7/liftOver/mm7ToCanFam2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm7','tetNig1','/gbdb/mm7/liftOver/mm7ToTetNig1.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm7','xenTro1','/gbdb/mm7/liftOver/mm7ToXenTro1.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm7','rn3','/gbdb/mm7/liftOver/mm7ToRn3.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm7','bosTau2','/gbdb/mm7/liftOver/mm7ToBosTau2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm7','panTro1','/gbdb/mm7/liftOver/mm7ToPanTro1.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm7','galGal2','/gbdb/mm7/liftOver/mm7ToGalGal2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm7','rheMac1','/gbdb/mm7/liftOver/mm7ToRheMac1.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm7','fr1','/gbdb/mm7/liftOver/mm7ToFr1.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm7','hg17','/gbdb/mm7/liftOver/mm7ToHg17.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('bosTau2','mm7','/gbdb/bosTau2/liftOver/bosTau2ToMm7.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('fr1','mm7','/gbdb/fr1/liftOver/fr1ToMm7.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('fr1','mm6','/gbdb/fr1/liftOver/fr1ToMm6.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('galGal2','mm6','/gbdb/galGal2/liftOver/galGal2ToMm6.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('galGal2','mm7','/gbdb/galGal2/liftOver/galGal2ToMm7.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('galGal2','danRer2','/gbdb/galGal2/liftOver/galGal2ToDanRer2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('galGal2','tetNig1','/gbdb/galGal2/liftOver/galGal2ToTetNig1.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('galGal2','hg17','/gbdb/galGal2/liftOver/galGal2ToHg17.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('galGal2','mm5','/gbdb/galGal2/liftOver/galGal2ToMm5.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('galGal2','xenTro1','/gbdb/galGal2/liftOver/galGal2ToXenTro1.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('galGal2','monDom1','/gbdb/galGal2/liftOver/galGal2ToMonDom1.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg17','mm7','/gbdb/hg17/liftOver/hg17ToMm7.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('panTro1','mm7','/gbdb/panTro1/liftOver/panTro1ToMm7.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('panTro1','mm6','/gbdb/panTro1/liftOver/panTro1ToMm6.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('panTro1','hg17','/gbdb/panTro1/liftOver/panTro1ToHg17.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('panTro1','hg16','/gbdb/panTro1/liftOver/panTro1ToHg16.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('rheMac1','mm7','/gbdb/rheMac1/liftOver/rheMac1ToMm7.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('rn3','mm7','/gbdb/rn3/liftOver/rn3ToMm7.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('tetNig1','mm6','/gbdb/tetNig1/liftOver/tetNig1ToMm6.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('tetNig1','mm7','/gbdb/tetNig1/liftOver/tetNig1ToMm7.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('xenTro1','mm6','/gbdb/xenTro1/liftOver/xenTro1ToMm6.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('xenTro1','mm7','/gbdb/xenTro1/liftOver/xenTro1ToMm7.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('dm2','anoGam1','/gbdb/dm2/liftOver/dm2ToAnoGam1.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('dm2','apiMel2','/gbdb/dm2/liftOver/dm2ToApiMel2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('dm2','dp3','/gbdb/dm2/liftOver/dm2ToDp3.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('dm2','droAna1','/gbdb/dm2/liftOver/dm2ToDroAna1.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('dm2','droAna2','/gbdb/dm2/liftOver/dm2ToDroAna2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('dm2','droEre1','/gbdb/dm2/liftOver/dm2ToDroEre1.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('dm2','droGri1','/gbdb/dm2/liftOver/dm2ToDroGri1.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('dm2','droMoj1','/gbdb/dm2/liftOver/dm2ToDroMoj1.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('dm2','droMoj2','/gbdb/dm2/liftOver/dm2ToDroMoj2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('dm2','droSim1','/gbdb/dm2/liftOver/dm2ToDroSim1.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('dm2','droVir1','/gbdb/dm2/liftOver/dm2ToDroVir1.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('dm2','droVir2','/gbdb/dm2/liftOver/dm2ToDroVir2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('dm2','droYak1','/gbdb/dm2/liftOver/dm2ToDroYak1.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('danRer1','danRer2','/gbdb/danRer1/liftOver/danRer1ToDanRer2.over.chain.gz',0.95,0,0,'N',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('danRer2','mm6','/gbdb/danRer2/liftOver/danRer2ToMm6.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg16','panTro1','/gbdb/hg16/liftOver/hg16ToPanTro1.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg16','galGal2','/gbdb/hg16/liftOver/hg16ToGalGal2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('droPer1','dm2','/gbdb/droPer1/liftOver/droPer1ToDm2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg16','rn3','/gbdb/hg16/liftOver/hg16ToRn3.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg17','hg18','/gbdb/hg17/liftOver/hg17ToHg18.over.chain.gz',0.95,0,0,'N',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg16','mm5','/gbdb/hg16/liftOver/hg16ToMm5.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg16','mm6','/gbdb/hg16/liftOver/hg16ToMm6.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('dm2','droPer1','/gbdb/dm2/liftOver/dm2ToDroPer1.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('droYak2','dm2','/gbdb/droYak2/liftOver/droYak2ToDm2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('dm2','droYak2','/gbdb/dm2/liftOver/dm2ToDroYak2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('droMoj2','dm2','/gbdb/droMoj2/liftOver/droMoj2ToDm2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('droAna2','dm2','/gbdb/droAna2/liftOver/droAna2ToDm2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('droEre1','dm2','/gbdb/droEre1/liftOver/droEre1ToDm2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('droSim1','dm2','/gbdb/droSim1/liftOver/droSim1ToDm2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('droVir2','dm2','/gbdb/droVir2/liftOver/droVir2ToDm2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('dp3','dm2','/gbdb/dp3/liftOver/dp3ToDm2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('droMoj1','dm2','/gbdb/droMoj1/liftOver/droMoj1ToDm2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('droAna1','dm2','/gbdb/droAna1/liftOver/droAna1ToDm2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('droVir1','dm2','/gbdb/droVir1/liftOver/droVir1ToDm2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('droYak1','dm2','/gbdb/droYak1/liftOver/droYak1ToDm2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('anoGam1','dm2','/gbdb/anoGam1/liftOver/anoGam1ToDm2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('apiMel2','dm2','/gbdb/apiMel2/liftOver/apiMel2ToDm2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('apiMel1','dm2','/gbdb/apiMel1/liftOver/apiMel1ToDm2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('droSec1','dm2','/gbdb/droSec1/liftOver/droSec1ToDm2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('dm2','droSec1','/gbdb/dm2/liftOver/dm2ToDroSec1.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('fr1','danRer3','/gbdb/fr1/liftOver/fr1ToDanRer3.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('danRer3','fr1','/gbdb/danRer3/liftOver/danRer3ToFr1.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('danRer3','tetNig1','/gbdb/danRer3/liftOver/danRer3ToTetNig1.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('danRer3','mm7','/gbdb/danRer3/liftOver/danRer3ToMm7.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm7','danRer3','/gbdb/mm7/liftOver/mm7ToDanRer3.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('tetNig1','danRer3','/gbdb/tetNig1/liftOver/tetNig1ToDanRer3.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg17','danRer3','/gbdb/hg17/liftOver/hg17ToDanRer3.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm7','mm5','/gbdb/mm7/liftOver/mm7ToMm5.over.chain.gz',0.95,0,0,'N',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('rheMac2','hg17','/gbdb/rheMac2/liftOver/rheMac2ToHg17.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg17','rheMac1','/gbdb/hg17/liftOver/hg17ToRheMac1.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('bosTau2','hg17','/gbdb/bosTau2/liftOver/bosTau2ToHg17.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg18','hg17','/gbdb/hg18/liftOver/hg18ToHg17.over.chain.gz',0.95,0,0,'N',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg18','fr1','/gbdb/hg18/liftOver/hg18ToFr1.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg18','rn3','/gbdb/hg18/liftOver/hg18ToRn3.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg18','mm7','/gbdb/hg18/liftOver/hg18ToMm7.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg18','danRer3','/gbdb/hg18/liftOver/hg18ToDanRer3.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg18','galGal2','/gbdb/hg18/liftOver/hg18ToGalGal2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg18','canFam2','/gbdb/hg18/liftOver/hg18ToCanFam2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg18','xenTro1','/gbdb/hg18/liftOver/hg18ToXenTro1.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg18','tetNig1','/gbdb/hg18/liftOver/hg18ToTetNig1.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg18','bosTau2','/gbdb/hg18/liftOver/hg18ToBosTau2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg18','panTro1','/gbdb/hg18/liftOver/hg18ToPanTro1.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg18','rheMac2','/gbdb/hg18/liftOver/hg18ToRheMac2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm8','hg18','/gbdb/mm8/liftOver/mm8ToHg18.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg18','mm8','/gbdb/hg18/liftOver/hg18ToMm8.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm8','rheMac2','/gbdb/mm8/liftOver/mm8ToRheMac2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm8','canFam2','/gbdb/mm8/liftOver/mm8ToCanFam2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm8','bosTau2','/gbdb/mm8/liftOver/mm8ToBosTau2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('canFam2','mm8','/gbdb/canFam2/liftOver/canFam2ToMm8.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('bosTau2','mm8','/gbdb/bosTau2/liftOver/bosTau2ToMm8.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('danRer2','danRer3','/gbdb/danRer2/liftOver/danRer2ToDanRer3.over.chain.gz',0.95,0,0,'N',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm8','fr1','/gbdb/mm8/liftOver/mm8ToFr1.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm8','tetNig1','/gbdb/mm8/liftOver/mm8ToTetNig1.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg17','rn4','/gbdb/hg17/liftOver/hg17ToRn4.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('fr1','mm8','/gbdb/fr1/liftOver/fr1ToMm8.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm8','galGal2','/gbdb/mm8/liftOver/mm8ToGalGal2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('galGal2','mm8','/gbdb/galGal2/liftOver/galGal2ToMm8.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('tetNig1','mm8','/gbdb/tetNig1/liftOver/tetNig1ToMm8.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('rheMac2','mm8','/gbdb/rheMac2/liftOver/rheMac2ToMm8.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm8','xenTro1','/gbdb/mm8/liftOver/mm8ToXenTro1.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('xenTro1','mm8','/gbdb/xenTro1/liftOver/xenTro1ToMm8.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm8','panTro1','/gbdb/mm8/liftOver/mm8ToPanTro1.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm8','danRer3','/gbdb/mm8/liftOver/mm8ToDanRer3.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('danRer3','mm8','/gbdb/danRer3/liftOver/danRer3ToMm8.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('panTro1','mm8','/gbdb/panTro1/liftOver/panTro1ToMm8.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm8','hg17','/gbdb/mm8/liftOver/mm8ToHg17.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg17','mm8','/gbdb/hg17/liftOver/hg17ToMm8.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm7','mm8','/gbdb/mm7/liftOver/mm7ToMm8.over.chain.gz',0.95,0,0,'N',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm8','mm7','/gbdb/mm8/liftOver/mm8ToMm7.over.chain.gz',0.95,0,0,'N',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('danRer3','danRer2','/gbdb/danRer3/liftOver/danRer3ToDanRer2.over.chain.gz',0.95,0,0,'N',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg16','hg18','/gbdb/hg16/liftOver/hg16ToHg18.over.chain.gz',0.95,0,0,'N',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('fr1','hg18','/gbdb/fr1/liftOver/fr1ToHg18.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('danRer3','hg18','/gbdb/danRer3/liftOver/danRer3ToHg18.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('galGal2','hg18','/gbdb/galGal2/liftOver/galGal2ToHg18.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('canFam2','hg18','/gbdb/canFam2/liftOver/canFam2ToHg18.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm7','hg18','/gbdb/mm7/liftOver/mm7ToHg18.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('xenTro1','hg18','/gbdb/xenTro1/liftOver/xenTro1ToHg18.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('tetNig1','hg18','/gbdb/tetNig1/liftOver/tetNig1ToHg18.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('bosTau2','hg18','/gbdb/bosTau2/liftOver/bosTau2ToHg18.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('panTro1','hg18','/gbdb/panTro1/liftOver/panTro1ToHg18.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('rheMac2','hg18','/gbdb/rheMac2/liftOver/rheMac2ToHg18.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('rn3','hg18','/gbdb/rn3/liftOver/rn3ToHg18.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('danRer4','fr1','/gbdb/danRer4/liftOver/danRer4ToFr1.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('danRer4','hg18','/gbdb/danRer4/liftOver/danRer4ToHg18.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('danRer4','tetNig1','/gbdb/danRer4/liftOver/danRer4ToTetNig1.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg18','danRer4','/gbdb/hg18/liftOver/hg18ToDanRer4.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('fr1','danRer4','/gbdb/fr1/liftOver/fr1ToDanRer4.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('tetNig1','danRer4','/gbdb/tetNig1/liftOver/tetNig1ToDanRer4.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('danRer4','mm8','/gbdb/danRer4/liftOver/danRer4ToMm8.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm8','danRer4','/gbdb/mm8/liftOver/mm8ToDanRer4.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('danRer3','danRer4','/gbdb/danRer3/liftOver/danRer3ToDanRer4.over.chain.gz',0.95,0,0,'N',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('danRer4','danRer3','/gbdb/danRer4/liftOver/danRer4ToDanRer3.over.chain.gz',0.95,0,0,'N',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm6','mm8','/gbdb/mm6/liftOver/mm6ToMm8.over.chain.gz',0.95,0,0,'N',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm5','mm8','/gbdb/mm5/liftOver/mm5ToMm8.over.chain.gz',0.95,0,0,'N',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('danRer4','xenTro2','/gbdb/danRer4/liftOver/danRer4ToXenTro2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('galGal2','xenTro2','/gbdb/galGal2/liftOver/galGal2ToXenTro2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg18','xenTro2','/gbdb/hg18/liftOver/hg18ToXenTro2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm8','xenTro2','/gbdb/mm8/liftOver/mm8ToXenTro2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm7','rn4','/gbdb/mm7/liftOver/mm7ToRn4.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('xenTro2','mm8','/gbdb/xenTro2/liftOver/xenTro2ToMm8.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('xenTro2','hg18','/gbdb/xenTro2/liftOver/xenTro2ToHg18.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('danRer3','rn4','/gbdb/danRer3/liftOver/danRer3ToRn4.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('xenTro2','galGal2','/gbdb/xenTro2/liftOver/xenTro2ToGalGal2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('xenTro2','danRer4','/gbdb/xenTro2/liftOver/xenTro2ToDanRer4.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm7','mm6','/gbdb/mm7/liftOver/mm7ToMm6.over.chain.gz',0.95,0,0,'N',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('galGal2','rn4','/gbdb/galGal2/liftOver/galGal2ToRn4.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('xenTro2','rn4','/gbdb/xenTro2/liftOver/xenTro2ToRn4.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('rheMac2','rn4','/gbdb/rheMac2/liftOver/rheMac2ToRn4.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm8','rn4','/gbdb/mm8/liftOver/mm8ToRn4.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg18','rn4','/gbdb/hg18/liftOver/hg18ToRn4.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('canFam2','rn4','/gbdb/canFam2/liftOver/canFam2ToRn4.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('bosTau2','rn4','/gbdb/bosTau2/liftOver/bosTau2ToRn4.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('rn4','xenTro1','/gbdb/rn4/liftOver/rn4ToXenTro1.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('rn4','rheMac2','/gbdb/rn4/liftOver/rn4ToRheMac2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('rn4','hg18','/gbdb/rn4/liftOver/rn4ToHg18.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('rn4','mm7','/gbdb/rn4/liftOver/rn4ToMm7.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('rn4','hg17','/gbdb/rn4/liftOver/rn4ToHg17.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('rn4','galGal2','/gbdb/rn4/liftOver/rn4ToGalGal2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('rn4','danRer3','/gbdb/rn4/liftOver/rn4ToDanRer3.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('rn4','canFam2','/gbdb/rn4/liftOver/rn4ToCanFam2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('rn4','bosTau2','/gbdb/rn4/liftOver/rn4ToBosTau2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('rn4','xenTro2','/gbdb/rn4/liftOver/rn4ToXenTro2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('rn4','mm8','/gbdb/rn4/liftOver/rn4ToMm8.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('rn3','rn4','/gbdb/rn3/liftOver/rn3ToRn4.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('danRer4','rn4','/gbdb/danRer4/liftOver/danRer4ToRn4.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('rn4','danRer4','/gbdb/rn4/liftOver/rn4ToDanRer4.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('panTro2','hg17','/gbdb/panTro2/liftOver/panTro2ToHg17.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('panTro2','hg18','/gbdb/panTro2/liftOver/panTro2ToHg18.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('panTro2','mm8','/gbdb/panTro2/liftOver/panTro2ToMm8.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('panTro2','panTro1','/gbdb/panTro2/liftOver/panTro2ToPanTro1.over.chain.gz',0.95,0,0,'N',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('panTro2','rheMac2','/gbdb/panTro2/liftOver/panTro2ToRheMac2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('panTro2','rn4','/gbdb/panTro2/liftOver/panTro2ToRn4.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg18','panTro2','/gbdb/hg18/liftOver/hg18ToPanTro2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('mm8','panTro2','/gbdb/mm8/liftOver/mm8ToPanTro2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('rheMac2','panTro2','/gbdb/rheMac2/liftOver/rheMac2ToPanTro2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('panTro1','panTro2','/gbdb/panTro1/liftOver/panTro1ToPanTro2.over.chain.gz',0.95,0,0,'N',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg17','panTro2','/gbdb/hg17/liftOver/hg17ToPanTro2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('rn4','panTro2','/gbdb/rn4/liftOver/rn4ToPanTro2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('canFam2','panTro2','/gbdb/canFam2/liftOver/canFam2ToPanTro2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('panTro2','canFam2','/gbdb/panTro2/liftOver/panTro2ToCanFam2.over.chain.gz',0.1,0,0,'Y',1,'N');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('fromDb','toDb','path',0,0,0,'m',0,'f');
INSERT INTO liftOverChain (fromDb, toDb, path, minMatch, minSizeT, minSizeQ, multiple, minBlocks, fudgeThick) VALUES ('hg17','hg15','/gbdb/hg17/liftOver/hg17ToHg15.over.chain.gz',0.95,0,0,'N',1,'N');

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
INSERT INTO genomeClade (genome, clade, priority) VALUES ('Rhesus','vertebrate',15);
INSERT INTO genomeClade (genome, clade, priority) VALUES ('D. simulans','insect',15);
INSERT INTO genomeClade (genome, clade, priority) VALUES ('D. erecta','insect',20);
INSERT INTO genomeClade (genome, clade, priority) VALUES ('D. grimshawi','insect',77);
INSERT INTO genomeClade (genome, clade, priority) VALUES ('D. persimilis','insect',55);
INSERT INTO genomeClade (genome, clade, priority) VALUES ('D. sechellia','insect',18);
INSERT INTO genomeClade (genome, clade, priority) VALUES ('S. purpuratus','deuterostome',20);

