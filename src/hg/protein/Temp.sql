-- MySQL dump 8.22
--
-- Host: localhost    Database: rn3Temp
---------------------------------------------------------
-- Server version	3.23.56

--
-- Table structure for table 'geneName'
--

CREATE TABLE geneName (
  id int(11) NOT NULL default '0',
  name longtext NOT NULL,
  PRIMARY KEY  (id),
  KEY name (name(16))
) TYPE=MyISAM;

--
-- Table structure for table 'history'
--

CREATE TABLE history (
  ix int(11) NOT NULL auto_increment,
  startId int(10) unsigned NOT NULL default '0',
  endId int(10) unsigned NOT NULL default '0',
  who varchar(255) NOT NULL default '',
  what varchar(255) NOT NULL default '',
  modTime timestamp(14) NOT NULL,
  PRIMARY KEY  (ix)
) TYPE=MyISAM;

--
-- Table structure for table 'keggList'
--

CREATE TABLE keggList (
  locusID varchar(40) NOT NULL default '',
  mapID varchar(40) NOT NULL default '',
  description varchar(255) NOT NULL default '',
  KEY locusID (locusID),
  KEY mapID (mapID)
) TYPE=MyISAM;

--
-- Table structure for table 'knownGene'
--

CREATE TABLE knownGene (
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
  proteinID varchar(40) NOT NULL default '',
  alignID varchar(8) NOT NULL default '',
  KEY name (name(10)),
  KEY chrom (chrom(12),txStart),
  KEY chrom_2 (chrom(12),txEnd),
  KEY protein (proteinID(12)),
  KEY align (alignID)
) TYPE=MyISAM;

--
-- Table structure for table 'knownGene0'
--

CREATE TABLE knownGene0 (
  name varchar(40) NOT NULL default '',
  chrom varchar(40) NOT NULL default '',
  strand char(1) NOT NULL default '',
  txStart int(10) unsigned NOT NULL default '0',
  txEnd int(10) unsigned NOT NULL default '0',
  cdsStart int(10) unsigned NOT NULL default '0',
  cdsEnd int(10) unsigned NOT NULL default '0',
  exonCount int(10) unsigned NOT NULL default '0',
  exonStarts longblob NOT NULL,
  exonEnds longblob NOT NULL,
  proteinID varchar(40) NOT NULL default '',
  alignID varchar(8) NOT NULL default '',
  KEY name (name(10)),
  KEY chrom (chrom(12),txStart),
  KEY chrom_2 (chrom(12),txEnd),
  KEY protein (proteinID(12)),
  KEY align (alignID)
) TYPE=MyISAM;

--
-- Table structure for table 'locus2Acc0'
--

CREATE TABLE locus2Acc0 (
  locusID int(11) NOT NULL default '0',
  gbAC varchar(255) NOT NULL default '',
  giNCBI varchar(255) NOT NULL default '',
  seqType varchar(255) NOT NULL default '',
  proteinAC varchar(255) NOT NULL default '',
  taxID int(11) NOT NULL default '0',
  KEY locusID (locusID),
  KEY i1 (giNCBI),
  KEY i5 (proteinAC),
  KEY i6 (gbAC)
) TYPE=MyISAM;

--
-- Table structure for table 'locus2Ref0'
--

CREATE TABLE locus2Ref0 (
  locusID int(11) NOT NULL default '0',
  refAC varchar(255) NOT NULL default '',
  giNCBI varchar(255) NOT NULL default '',
  revStatus varchar(255) NOT NULL default '',
  proteinAC varchar(255) NOT NULL default '',
  taxID int(11) NOT NULL default '0',
  KEY locusID (locusID),
  KEY refAC (refAC),
  KEY giNCBI (giNCBI),
  KEY proteinAC (proteinAC)
) TYPE=MyISAM;


--
-- Table structure for table 'mrnaGene'
--

CREATE TABLE mrnaGene (
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
  KEY name (name(10)),
  KEY chrom (chrom(12),txStart),
  KEY chrom_2 (chrom(12),txEnd)
) TYPE=MyISAM;

--
-- Table structure for table 'pepSequence'
--

CREATE TABLE pepSequence (
  bioentry_id int(10) unsigned NOT NULL auto_increment,
  biodatabase_id int(10) NOT NULL default '0',
  display_id varchar(40) NOT NULL default '',
  accession varchar(40) NOT NULL default '',
  division char(3) NOT NULL default '',
  genenames varchar(40) default NULL,
  desc1 varchar(255) default NULL,
  desc2 varchar(255) default NULL,
  biosequence_str mediumtext,
  PRIMARY KEY  (bioentry_id),
  UNIQUE KEY biodatabase_id (biodatabase_id,accession,division),
  KEY bioentrydbid (biodatabase_id),
  KEY bioentrydid (display_id),
  KEY bioentryacc (accession)
) TYPE=MyISAM;

--
-- Table structure for table 'productName'
--

CREATE TABLE productName (
  id int(11) NOT NULL default '0',
  name longtext NOT NULL,
  PRIMARY KEY  (id),
  KEY name (name(16))
) TYPE=MyISAM;

--
-- Table structure for table 'refGene'
--

CREATE TABLE refGene (
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
  KEY name (name(10)),
  KEY chrom (chrom(12),txStart),
  KEY chrom_2 (chrom(12),txEnd)
) TYPE=MyISAM;

--
-- Table structure for table 'refLink'
--

CREATE TABLE refLink (
  name varchar(255) NOT NULL default '',
  product varchar(255) NOT NULL default '',
  mrnaAcc varchar(255) NOT NULL default '',
  protAcc varchar(255) NOT NULL default '',
  geneName int(10) unsigned NOT NULL default '0',
  prodName int(10) unsigned NOT NULL default '0',
  locusLinkId int(10) unsigned NOT NULL default '0',
  omimId int(10) unsigned NOT NULL default '0',
  PRIMARY KEY  (mrnaAcc(12)),
  KEY name (name(10)),
  KEY protAcc (protAcc(10)),
  KEY locusLinkId (locusLinkId),
  KEY omimId (omimId),
  KEY prodName (prodName),
  KEY geneName (geneName)
) TYPE=MyISAM;

--
-- Table structure for table 'refMrna'
--

CREATE TABLE refMrna (
  name varchar(255) NOT NULL default '',
  seq longblob NOT NULL,
  PRIMARY KEY  (name(32))
) TYPE=MyISAM;

--
-- Table structure for table 'refPep'
--

CREATE TABLE refPep (
  name varchar(255) NOT NULL default '',
  seq longblob NOT NULL,
  PRIMARY KEY  (name(32))
) TYPE=MyISAM;

--
-- Table structure for table 'spMrna'
--

CREATE TABLE spMrna (
  spID varchar(255) NOT NULL default '',
  mrnaID varchar(255) NOT NULL default '',
  KEY spID (spID),
  KEY mrnaID (mrnaID)
) TYPE=MyISAM;

