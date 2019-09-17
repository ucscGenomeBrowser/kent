-- MySQL dump 10.15  Distrib 10.0.36-MariaDB, for Linux (x86_64)
--
-- Host: localhost    Database: kentXDjTstWhyYDropMe
-- ------------------------------------------------------
-- Server version	10.0.36-MariaDB

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8 */;
/*!40103 SET @OLD_TIME_ZONE=@@TIME_ZONE */;
/*!40103 SET TIME_ZONE='+00:00' */;
/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;

--
-- Table structure for table `hcat_assaytech`
--

DROP TABLE IF EXISTS `hcat_assaytech`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_assaytech` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `short_name` varchar(50) NOT NULL,
  `description` varchar(250) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_assaytech_short_name_fc7a5a4b_uniq` (`short_name`)
) ENGINE=MyISAM AUTO_INCREMENT=7 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_assaytech`
--

LOCK TABLES `hcat_assaytech` WRITE;
/*!40000 ALTER TABLE `hcat_assaytech` DISABLE KEYS */;
INSERT INTO `hcat_assaytech` VALUES (1,'10x Chromium V2','10x single-cell sequencing Chromium V2 chemistry'),(2,'smart-seq2 PE','smart-seq2 library prep paired end sequencing of single cells'),(3,'smart-seq2 SE','smart-seq2 library prep single ended sequencing of single cells'),(4,'bulk RNA','bulk RNA sequencing of some sort, not single cell'),(5,'scATAC-seq','Single cell ATAC-seq'),(6,'bulk DNA','sequencing of bulk, non-single-cell, DNA');
/*!40000 ALTER TABLE `hcat_assaytech` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_assaytech_comments`
--

DROP TABLE IF EXISTS `hcat_assaytech_comments`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_assaytech_comments` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `assaytech_id` int(11) NOT NULL,
  `comment_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_assaytech_comm_assaytech_id_c_b7d6a08e_uniq` (`assaytech_id`,`comment_id`),
  KEY `hcat_assaytech_comments_assaytech_id_05f1b7d7` (`assaytech_id`),
  KEY `hcat_assaytech_comments_comment_id_99270010` (`comment_id`)
) ENGINE=MyISAM AUTO_INCREMENT=5 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_assaytech_comments`
--

LOCK TABLES `hcat_assaytech_comments` WRITE;
/*!40000 ALTER TABLE `hcat_assaytech_comments` DISABLE KEYS */;
INSERT INTO `hcat_assaytech_comments` VALUES (1,6,13),(2,4,13),(3,4,9),(4,5,2);
/*!40000 ALTER TABLE `hcat_assaytech_comments` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_assaytech_projects`
--

DROP TABLE IF EXISTS `hcat_assaytech_projects`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_assaytech_projects` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `assaytech_id` int(11) NOT NULL,
  `project_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_assaytech_projects_assaytech_id_project_id_82eea493_uniq` (`assaytech_id`,`project_id`),
  KEY `hcat_assaytech_projects_assaytech_id_47be4cfd` (`assaytech_id`),
  KEY `hcat_assaytech_projects_project_id_209d1898` (`project_id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_assaytech_projects`
--

LOCK TABLES `hcat_assaytech_projects` WRITE;
/*!40000 ALTER TABLE `hcat_assaytech_projects` DISABLE KEYS */;
/*!40000 ALTER TABLE `hcat_assaytech_projects` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_assaytype`
--

DROP TABLE IF EXISTS `hcat_assaytype`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_assaytype` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `short_name` varchar(50) NOT NULL,
  `description` varchar(250) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_assaytype_short_name_05201662_uniq` (`short_name`)
) ENGINE=MyISAM AUTO_INCREMENT=6 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_assaytype`
--

LOCK TABLES `hcat_assaytype` WRITE;
/*!40000 ALTER TABLE `hcat_assaytype` DISABLE KEYS */;
INSERT INTO `hcat_assaytype` VALUES (1,'RNA levels','A measurement of the level of RNA within the sample.'),(2,'open chromatin','A chromatin accessibility assay to locate regulatory regions such as DNAse-seq or ATAC-seq'),(4,'histone modification','An assay aimed at finding the state of histone modifications along the genome.'),(5,'lineage tracing','Methods to mark a cells so as to trace their descendents.');
/*!40000 ALTER TABLE `hcat_assaytype` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_assaytype_comments`
--

DROP TABLE IF EXISTS `hcat_assaytype_comments`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_assaytype_comments` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `assaytype_id` int(11) NOT NULL,
  `comment_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_assaytype_comm_assaytype_id_c_b1abd9bb_uniq` (`assaytype_id`,`comment_id`),
  KEY `hcat_assaytype_comments_assaytype_id_473fa7e3` (`assaytype_id`),
  KEY `hcat_assaytype_comments_comment_id_2a57b15f` (`comment_id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_assaytype_comments`
--

LOCK TABLES `hcat_assaytype_comments` WRITE;
/*!40000 ALTER TABLE `hcat_assaytype_comments` DISABLE KEYS */;
/*!40000 ALTER TABLE `hcat_assaytype_comments` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_assaytype_projects`
--

DROP TABLE IF EXISTS `hcat_assaytype_projects`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_assaytype_projects` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `assaytype_id` int(11) NOT NULL,
  `project_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_assaytype_projects_assaytype_id_project_id_f1b2e52f_uniq` (`assaytype_id`,`project_id`),
  KEY `hcat_assaytype_projects_assaytype_id_7aa442b0` (`assaytype_id`),
  KEY `hcat_assaytype_projects_project_id_e24c8b51` (`project_id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_assaytype_projects`
--

LOCK TABLES `hcat_assaytype_projects` WRITE;
/*!40000 ALTER TABLE `hcat_assaytype_projects` DISABLE KEYS */;
/*!40000 ALTER TABLE `hcat_assaytype_projects` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_comment`
--

DROP TABLE IF EXISTS `hcat_comment`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_comment` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `text` varchar(255) NOT NULL,
  `type_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `text` (`text`),
  KEY `hcat_comment_type_id_92e157ad` (`type_id`)
) ENGINE=MyISAM AUTO_INCREMENT=20 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_comment`
--

LOCK TABLES `hcat_comment` WRITE;
/*!40000 ALTER TABLE `hcat_comment` DISABLE KEYS */;
INSERT INTO `hcat_comment` VALUES (1,'Test set doesn\'t need to really make sense',1),(2,'Not sure if  I\'ll keep this',1),(3,'Jim, what is going on?',2),(4,'This is going away soon',3),(5,'Test set in real database?',3),(6,'So if any piece breaks we all stop work?',2),(7,'on vacation',5),(8,'Does not know role as lab contact yet',3),(9,'It works!',5),(10,'They got mad at me',6),(11,'very patient',5),(12,'faked grant id',3),(13,'Would need more detail if anybody cared',1),(14,'Split EBI vs UCSC?',1),(15,'from GSE103354',1),(16,'This could be simplified',1),(17,'Wish I didn\'t have to do this',7),(18,'Isn\'t there more to this data set?',1),(19,'Ask Clay',1);
/*!40000 ALTER TABLE `hcat_comment` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_comment_files`
--

DROP TABLE IF EXISTS `hcat_comment_files`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_comment_files` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `comment_id` int(11) NOT NULL,
  `file_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_comment_files_comment_id_fil_b704cb5e_uniq` (`comment_id`,`file_id`),
  KEY `hcat_comment_files_comment_id_70dc34ff` (`comment_id`),
  KEY `hcat_comment_files_file_id_5993c9be` (`file_id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_comment_files`
--

LOCK TABLES `hcat_comment_files` WRITE;
/*!40000 ALTER TABLE `hcat_comment_files` DISABLE KEYS */;
/*!40000 ALTER TABLE `hcat_comment_files` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_comment_urls`
--

DROP TABLE IF EXISTS `hcat_comment_urls`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_comment_urls` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `comment_id` int(11) NOT NULL,
  `url_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_comment_urls_comment_id_url_id_03792934_uniq` (`comment_id`,`url_id`),
  KEY `hcat_comment_urls_comment_id_eb8602bc` (`comment_id`),
  KEY `hcat_comment_urls_url_id_9fd66331` (`url_id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_comment_urls`
--

LOCK TABLES `hcat_comment_urls` WRITE;
/*!40000 ALTER TABLE `hcat_comment_urls` DISABLE KEYS */;
/*!40000 ALTER TABLE `hcat_comment_urls` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_commenttype`
--

DROP TABLE IF EXISTS `hcat_commenttype`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_commenttype` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `short_name` varchar(10) NOT NULL,
  `description` longtext NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `short_name` (`short_name`)
) ENGINE=MyISAM AUTO_INCREMENT=8 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_commenttype`
--

LOCK TABLES `hcat_commenttype` WRITE;
/*!40000 ALTER TABLE `hcat_commenttype` DISABLE KEYS */;
INSERT INTO `hcat_commenttype` VALUES (1,'note','A comment worth knowing'),(2,'wtf','Someone please explain this to me, it doesn\'t seem right.'),(3,'warn','A comment about something that could go wrong'),(4,'bug','A problem that needs to be fixed'),(5,'glad','A comment about something that makes you happy'),(6,'sad','A comment about something that makes you sad'),(7,'mad','A comment about something that makes you mad');
/*!40000 ALTER TABLE `hcat_commenttype` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_consent`
--

DROP TABLE IF EXISTS `hcat_consent`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_consent` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `short_name` varchar(50) NOT NULL,
  `description` varchar(250) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_consent_short_name_9a57e804_uniq` (`short_name`)
) ENGINE=MyISAM AUTO_INCREMENT=7 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_consent`
--

LOCK TABLES `hcat_consent` WRITE;
/*!40000 ALTER TABLE `hcat_consent` DISABLE KEYS */;
INSERT INTO `hcat_consent` VALUES (1,'already openly published','This dataset already exists on the internet in a public manner,  perhaps at EBI\'s ENA or NCBI\'s SRA or GEO'),(2,'nonhuman data','Data is from mice or other model organisms'),(3,'consented','Consented human data'),(4,'dubious','Someone has looked at the consent and been unable to determine if it is sufficient.  Further research advised.'),(5,'unconsented','Don\'t work on unconsented stuff, it\'ll break your heart'),(6,'human no sequence','Concented to share expression matrix and higher level stuff but not reads.');
/*!40000 ALTER TABLE `hcat_consent` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_consent_comments`
--

DROP TABLE IF EXISTS `hcat_consent_comments`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_consent_comments` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `consent_id` int(11) NOT NULL,
  `comment_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_consent_commen_consent_id_com_76d4e001_uniq` (`consent_id`,`comment_id`),
  KEY `hcat_consent_comments_consent_id_e5172468` (`consent_id`),
  KEY `hcat_consent_comments_comment_id_44f3bb6f` (`comment_id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_consent_comments`
--

LOCK TABLES `hcat_consent_comments` WRITE;
/*!40000 ALTER TABLE `hcat_consent_comments` DISABLE KEYS */;
/*!40000 ALTER TABLE `hcat_consent_comments` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_contributor`
--

DROP TABLE IF EXISTS `hcat_contributor`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_contributor` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `name` varchar(250) NOT NULL,
  `email` varchar(254) NOT NULL,
  `phone` varchar(25) NOT NULL,
  `project_role` varchar(100) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_contributor_name_7ba24c2b_uniq` (`name`)
) ENGINE=MyISAM AUTO_INCREMENT=57 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_contributor`
--

LOCK TABLES `hcat_contributor` WRITE;
/*!40000 ALTER TABLE `hcat_contributor` DISABLE KEYS */;
INSERT INTO `hcat_contributor` VALUES (1,'Asa,Kristina,Bjorklund','asa.bjorklund@scilifelab.se','','lab contact'),(2,'Mjösberg,J,M','','','PI'),(3,'Clay,M,Fischer','lmfisch@ucsc.edu','','wrangler'),(4,'W,Jim,Kent','jimkent@ucsc.edu','','wrangler'),(5,'Paris,,Nejad','pnejad@ucsc.edu','','wrangler'),(6,'William,,Sullivan','wisulliv@ucsc.edu','','wrangler'),(7,'Chris,,Villareal','cjvillar@ucsc.edu','','wrangler'),(8,'Alex,,Pollen','Alex.Pollen@ucsf.edu','','lab contact'),(9,'Evangelia,,Diamanti','','','contributor'),(10,'Fiona,,Hamey','','','contributor'),(11,'Dimitris,,Karamitros','','','contributor'),(12,'Bilyana,,Stoilova','','','contributor'),(13,'Lindsey,W,Plasschaert','','','contributor'),(14,'Rapolas,,Zilionis','','','contributor'),(15,'Rayman,,Choo-Wing','','','contributor'),(16,'Virginia,,Savova','','','contributor'),(17,'Judith,,Knehr','','','contributor'),(18,'Guglielmo,,Roma','','','contributor'),(19,'Allon,M,Klein','','','contributor'),(20,'Aron,B,Jaffe','','','contributor'),(21,'Simone,,Picelli','','','contributor'),(22,'Marianne,,Forkel','','','contributor'),(23,'Rickard,,Sandberg','','','contributor'),(24,'Spyros,,Darmanis','','','lab contact'),(25,'Stephen,R,Quake','','','PI'),(26,'Daniel,T,Montoro','','','contributor'),(27,'Adam,L,Haber','','','contributor'),(28,'Moshe,,Biton','','','contributor'),(29,'Vladimir,,Vinarsky','','','contributor'),(30,'Sijia,,Chen','','','contributor'),(31,'Jorge,,Villoria','','','contributor'),(32,'Noga,,Rogel','','','contributor'),(33,'Purushothama,,Rao Tata','','','contributor'),(34,'Steven,M,Rowe','','','contributor'),(35,'John,F,Engelhardt','','','contributor'),(37,'Aviv,,Regev','','','PI'),(38,'Jayaraj,,Rajagopal','','','contributor'),(39,'Brian,,Lin','','','contributor'),(40,'Susan,,Birket','','','contributor'),(41,'Feng,,Yuan','','','contributor'),(42,'Hui Min,,Leung','','','contributor'),(43,'Grace,,Burgin','','','contributor'),(44,'Alexander,,Tsankov','','','contributor'),(45,'Avinash,,Waghray','','','contributor'),(46,'Michal,,Slyper','','','contributor'),(47,'Julia,,Waldman','','','contributor'),(48,'Lan,,Nguyen','','','contributor'),(49,'Danielle,,Dionne','','','contributor'),(50,'Orit,,Rozenblatt-Rosen','','','contributor'),(51,'Hongmei,,Mou','','','contributor'),(52,'Manjunatha,,Shivaraju','','','contributor'),(53,'Herman,,Bihler','','','contributor'),(54,'Martin,,Mense','','','contributor'),(55,'Guillermo,,Tearney','','','contributor'),(56,'Maximillian,,Haeussler','max@soe.ucsc.edu','','wrangler');
/*!40000 ALTER TABLE `hcat_contributor` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_contributor_comments`
--

DROP TABLE IF EXISTS `hcat_contributor_comments`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_contributor_comments` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `contributor_id` int(11) NOT NULL,
  `comment_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_contributor_comment_contributor_id_comm_31485906_uniq` (`contributor_id`,`comment_id`),
  KEY `hcat_contributor_comments_contributor_id_2310d420` (`contributor_id`),
  KEY `hcat_contributor_comments_comment_id_70093d4f` (`comment_id`)
) ENGINE=MyISAM AUTO_INCREMENT=5 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_contributor_comments`
--

LOCK TABLES `hcat_contributor_comments` WRITE;
/*!40000 ALTER TABLE `hcat_contributor_comments` DISABLE KEYS */;
INSERT INTO `hcat_contributor_comments` VALUES (1,56,7),(2,8,8),(3,4,9),(4,4,3);
/*!40000 ALTER TABLE `hcat_contributor_comments` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_contributor_grants`
--

DROP TABLE IF EXISTS `hcat_contributor_grants`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_contributor_grants` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `contributor_id` int(11) NOT NULL,
  `grant_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_contributor_grants_contributor_id_grant_id_c1db5ff4_uniq` (`contributor_id`,`grant_id`),
  KEY `hcat_contributor_grants_contributor_id_72008a75` (`contributor_id`),
  KEY `hcat_contributor_grants_grant_id_d9a8eb67` (`grant_id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_contributor_grants`
--

LOCK TABLES `hcat_contributor_grants` WRITE;
/*!40000 ALTER TABLE `hcat_contributor_grants` DISABLE KEYS */;
/*!40000 ALTER TABLE `hcat_contributor_grants` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_contributor_labs`
--

DROP TABLE IF EXISTS `hcat_contributor_labs`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_contributor_labs` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `contributor_id` int(11) NOT NULL,
  `lab_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_contributor_labs_contributor_id_lab_id_6f7c468b_uniq` (`contributor_id`,`lab_id`),
  KEY `hcat_contributor_labs_contributor_id_374a40f1` (`contributor_id`),
  KEY `hcat_contributor_labs_lab_id_e5eb138c` (`lab_id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_contributor_labs`
--

LOCK TABLES `hcat_contributor_labs` WRITE;
/*!40000 ALTER TABLE `hcat_contributor_labs` DISABLE KEYS */;
/*!40000 ALTER TABLE `hcat_contributor_labs` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_contributor_projects`
--

DROP TABLE IF EXISTS `hcat_contributor_projects`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_contributor_projects` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `contributor_id` int(11) NOT NULL,
  `project_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_contributor_project_contributor_id_project_i_da2fb502_uniq` (`contributor_id`,`project_id`),
  KEY `hcat_contributor_projects_contributor_id_e8b912b7` (`contributor_id`),
  KEY `hcat_contributor_projects_project_id_394a7b89` (`project_id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_contributor_projects`
--

LOCK TABLES `hcat_contributor_projects` WRITE;
/*!40000 ALTER TABLE `hcat_contributor_projects` DISABLE KEYS */;
/*!40000 ALTER TABLE `hcat_contributor_projects` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_disease`
--

DROP TABLE IF EXISTS `hcat_disease`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_disease` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `short_name` varchar(50) NOT NULL,
  `description` varchar(250) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_disease_short_name_f5dc3e71_uniq` (`short_name`)
) ENGINE=MyISAM AUTO_INCREMENT=8 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_disease`
--

LOCK TABLES `hcat_disease` WRITE;
/*!40000 ALTER TABLE `hcat_disease` DISABLE KEYS */;
INSERT INTO `hcat_disease` VALUES (1,'obstructive sleep apnea','Difficulties breathing while sleeping that sometimes can be relieved by tonsilectomy'),(2,'normal','Healthy donor and specimen'),(3,'type 2 diabetes','Type 2 diabetes mellitus - high blood sugar levels from insulin insensitivity'),(4,'cardiovascular disease','A disease of the heart, arteries, veins, and lungs'),(5,'parkinsons','Parkinson\'s disease - tremors and stiffness from atrophy of substantia negra'),(6,'hepatic steatosis','hepatic steatosis, also known as fatty liver, effects 1% of the USA'),(7,'healthy','NEEDS DESCRIPTION');
/*!40000 ALTER TABLE `hcat_disease` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_disease_comments`
--

DROP TABLE IF EXISTS `hcat_disease_comments`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_disease_comments` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `disease_id` int(11) NOT NULL,
  `comment_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_disease_commen_disease_id_com_72e84bd0_uniq` (`disease_id`,`comment_id`),
  KEY `hcat_disease_comments_disease_id_450319a9` (`disease_id`),
  KEY `hcat_disease_comments_comment_id_7c08e15f` (`comment_id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_disease_comments`
--

LOCK TABLES `hcat_disease_comments` WRITE;
/*!40000 ALTER TABLE `hcat_disease_comments` DISABLE KEYS */;
/*!40000 ALTER TABLE `hcat_disease_comments` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_disease_projects`
--

DROP TABLE IF EXISTS `hcat_disease_projects`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_disease_projects` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `disease_id` int(11) NOT NULL,
  `project_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_disease_projects_disease_id_project_id_027ff738_uniq` (`disease_id`,`project_id`),
  KEY `hcat_disease_projects_disease_id_a4d5d498` (`disease_id`),
  KEY `hcat_disease_projects_project_id_f88f39b1` (`project_id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_disease_projects`
--

LOCK TABLES `hcat_disease_projects` WRITE;
/*!40000 ALTER TABLE `hcat_disease_projects` DISABLE KEYS */;
/*!40000 ALTER TABLE `hcat_disease_projects` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_efforttype`
--

DROP TABLE IF EXISTS `hcat_efforttype`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_efforttype` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `short_name` varchar(50) NOT NULL,
  `description` varchar(255) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `short_name` (`short_name`)
) ENGINE=MyISAM AUTO_INCREMENT=3 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_efforttype`
--

LOCK TABLES `hcat_efforttype` WRITE;
/*!40000 ALTER TABLE `hcat_efforttype` DISABLE KEYS */;
INSERT INTO `hcat_efforttype` VALUES (1,'import GEO','Imported from NCBI\'s Gene Expression Omnibus (GEO) using UCSC Strex pipeline'),(2,'live wrangling','Imported into hca by live HCA-sponsered wrangling efforts');
/*!40000 ALTER TABLE `hcat_efforttype` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_efforttype_comments`
--

DROP TABLE IF EXISTS `hcat_efforttype_comments`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_efforttype_comments` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `efforttype_id` int(11) NOT NULL,
  `comment_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_projectorigin__projectorigin_id_vo_b58128f3_uniq` (`efforttype_id`,`comment_id`),
  KEY `hcat_projectorigin_comments_projectorigin_id_75637af6` (`efforttype_id`),
  KEY `hcat_projectorigin_comments_comment_id_80c16dc8` (`comment_id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_efforttype_comments`
--

LOCK TABLES `hcat_efforttype_comments` WRITE;
/*!40000 ALTER TABLE `hcat_efforttype_comments` DISABLE KEYS */;
/*!40000 ALTER TABLE `hcat_efforttype_comments` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_file`
--

DROP TABLE IF EXISTS `hcat_file`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_file` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `short_name` varchar(80) NOT NULL,
  `file` varchar(100) NOT NULL,
  `description` varchar(255) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `short_name` (`short_name`)
) ENGINE=MyISAM AUTO_INCREMENT=3 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_file`
--

LOCK TABLES `hcat_file` WRITE;
/*!40000 ALTER TABLE `hcat_file` DISABLE KEYS */;
INSERT INTO `hcat_file` VALUES (1,'Full empty template for v6 metadata','uploads/hca_allFields.xlsx','An excel file that contains all tabs that anyone might ever want as of mid 2019');
/*!40000 ALTER TABLE `hcat_file` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_file_comments`
--

DROP TABLE IF EXISTS `hcat_file_comments`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_file_comments` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `file_id` int(11) NOT NULL,
  `comment_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_file_comments_file_id_commen_49212233_uniq` (`file_id`,`comment_id`),
  KEY `hcat_file_comments_file_id_4fbc89de` (`file_id`),
  KEY `hcat_file_comments_comment_id_d36929e1` (`comment_id`)
) ENGINE=MyISAM AUTO_INCREMENT=2 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_file_comments`
--

LOCK TABLES `hcat_file_comments` WRITE;
/*!40000 ALTER TABLE `hcat_file_comments` DISABLE KEYS */;
INSERT INTO `hcat_file_comments` VALUES (1,1,16);
/*!40000 ALTER TABLE `hcat_file_comments` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_funder`
--

DROP TABLE IF EXISTS `hcat_funder`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_funder` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `short_name` varchar(50) NOT NULL,
  `description` longtext NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=MyISAM AUTO_INCREMENT=7 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_funder`
--

LOCK TABLES `hcat_funder` WRITE;
/*!40000 ALTER TABLE `hcat_funder` DISABLE KEYS */;
INSERT INTO `hcat_funder` VALUES (1,'Swedish Research Council','Funds research in Sweden.  See https://www.vr.se/english.html'),(2,'NIH','National Institutes of Health of the Unites States of America'),(3,'CZI','Chan-Zuckerberg Initiative'),(4,'MRC','United Kingdom\'s Medical Research Council'),(5,'Gates','Gates Foundation'),(6,'anonymous','A donor who is humble enough not to be pestered by further requests for more funds.');
/*!40000 ALTER TABLE `hcat_funder` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_funder_comments`
--

DROP TABLE IF EXISTS `hcat_funder_comments`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_funder_comments` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `funder_id` int(11) NOT NULL,
  `comment_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_funder_comment_funder_id_comm_f1663a0a_uniq` (`funder_id`,`comment_id`),
  KEY `hcat_funder_comments_funder_id_ff2a5db5` (`funder_id`),
  KEY `hcat_funder_comments_comment_id_bd916527` (`comment_id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_funder_comments`
--

LOCK TABLES `hcat_funder_comments` WRITE;
/*!40000 ALTER TABLE `hcat_funder_comments` DISABLE KEYS */;
/*!40000 ALTER TABLE `hcat_funder_comments` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_grant`
--

DROP TABLE IF EXISTS `hcat_grant`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_grant` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `grant_title` varchar(200) NOT NULL,
  `grant_id` varchar(200) NOT NULL,
  `funder_id` int(11) DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `hcat_grant_funder_id_a47964f4` (`funder_id`)
) ENGINE=MyISAM AUTO_INCREMENT=4 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_grant`
--

LOCK TABLES `hcat_grant` WRITE;
/*!40000 ALTER TABLE `hcat_grant` DISABLE KEYS */;
INSERT INTO `hcat_grant` VALUES (2,'anonymous gift','gift_0001',6),(3,'HCA-DCP UC Santa Cruz','CZI # 2017-171531',3);
/*!40000 ALTER TABLE `hcat_grant` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_grant_comments`
--

DROP TABLE IF EXISTS `hcat_grant_comments`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_grant_comments` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `grant_id` int(11) NOT NULL,
  `comment_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_grant_comments_grant_id_comment_id_75857c4d_uniq` (`grant_id`,`comment_id`),
  KEY `hcat_grant_comments_grant_id_32c0a6e3` (`grant_id`),
  KEY `hcat_grant_comments_comment_id_6a695cf1` (`comment_id`)
) ENGINE=MyISAM AUTO_INCREMENT=3 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_grant_comments`
--

LOCK TABLES `hcat_grant_comments` WRITE;
/*!40000 ALTER TABLE `hcat_grant_comments` DISABLE KEYS */;
INSERT INTO `hcat_grant_comments` VALUES (1,2,11);
/*!40000 ALTER TABLE `hcat_grant_comments` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_grant_funded_contributors`
--

DROP TABLE IF EXISTS `hcat_grant_funded_contributors`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_grant_funded_contributors` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `grant_id` int(11) NOT NULL,
  `contributor_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_grant_funded_contri_grant_id_contributor_id_4b64935e_uniq` (`grant_id`,`contributor_id`),
  KEY `hcat_grant_funded_contributors_grant_id_448f7bc3` (`grant_id`),
  KEY `hcat_grant_funded_contributors_contributor_id_0015c0ac` (`contributor_id`)
) ENGINE=MyISAM AUTO_INCREMENT=10 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_grant_funded_contributors`
--

LOCK TABLES `hcat_grant_funded_contributors` WRITE;
/*!40000 ALTER TABLE `hcat_grant_funded_contributors` DISABLE KEYS */;
INSERT INTO `hcat_grant_funded_contributors` VALUES (3,2,4),(4,3,3),(5,3,4),(6,3,5),(7,3,6),(8,3,7),(9,3,56);
/*!40000 ALTER TABLE `hcat_grant_funded_contributors` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_grant_funded_labs`
--

DROP TABLE IF EXISTS `hcat_grant_funded_labs`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_grant_funded_labs` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `grant_id` int(11) NOT NULL,
  `lab_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_grant_funded_labs_grant_id_lab_id_0c7bf27e_uniq` (`grant_id`,`lab_id`),
  KEY `hcat_grant_funded_labs_grant_id_517887b0` (`grant_id`),
  KEY `hcat_grant_funded_labs_lab_id_87e7b483` (`lab_id`)
) ENGINE=MyISAM AUTO_INCREMENT=4 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_grant_funded_labs`
--

LOCK TABLES `hcat_grant_funded_labs` WRITE;
/*!40000 ALTER TABLE `hcat_grant_funded_labs` DISABLE KEYS */;
INSERT INTO `hcat_grant_funded_labs` VALUES (3,3,3);
/*!40000 ALTER TABLE `hcat_grant_funded_labs` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_grant_funded_projects`
--

DROP TABLE IF EXISTS `hcat_grant_funded_projects`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_grant_funded_projects` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `grant_id` int(11) NOT NULL,
  `project_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_grant_funded_projects_grant_id_project_id_53c1372f_uniq` (`grant_id`,`project_id`),
  KEY `hcat_grant_funded_projects_grant_id_b65d47bb` (`grant_id`),
  KEY `hcat_grant_funded_projects_project_id_4db04f16` (`project_id`)
) ENGINE=MyISAM AUTO_INCREMENT=3 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_grant_funded_projects`
--

LOCK TABLES `hcat_grant_funded_projects` WRITE;
/*!40000 ALTER TABLE `hcat_grant_funded_projects` DISABLE KEYS */;
/*!40000 ALTER TABLE `hcat_grant_funded_projects` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_lab`
--

DROP TABLE IF EXISTS `hcat_lab`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_lab` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `short_name` varchar(50) NOT NULL,
  `institution` varchar(250) NOT NULL,
  `contact_id` int(11) DEFAULT NULL,
  `pi_id` int(11) DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_lab_short_name_b6009c0a_uniq` (`short_name`),
  KEY `hcat_lab_contact_id_a8a756d6` (`contact_id`),
  KEY `hcat_lab_pi_id_ef656759` (`pi_id`)
) ENGINE=MyISAM AUTO_INCREMENT=5 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_lab`
--

LOCK TABLES `hcat_lab` WRITE;
/*!40000 ALTER TABLE `hcat_lab` DISABLE KEYS */;
INSERT INTO `hcat_lab` VALUES (1,'MjösbergLabAtUppsala','Uppsala University',1,2),(2,'CziBioHub','Bio Hub',24,25),(3,'kent','UC Santa Cruz',3,4),(4,'Gottgens University of Cambridge','University of Cambridge',9,NULL);
/*!40000 ALTER TABLE `hcat_lab` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_lab_comments`
--

DROP TABLE IF EXISTS `hcat_lab_comments`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_lab_comments` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `lab_id` int(11) NOT NULL,
  `comment_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_lab_comments_lab_id_comment_id_288ed1ef_uniq` (`lab_id`,`comment_id`),
  KEY `hcat_lab_comments_lab_id_98041c3d` (`lab_id`),
  KEY `hcat_lab_comments_comment_id_5e62e5b7` (`comment_id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_lab_comments`
--

LOCK TABLES `hcat_lab_comments` WRITE;
/*!40000 ALTER TABLE `hcat_lab_comments` DISABLE KEYS */;
/*!40000 ALTER TABLE `hcat_lab_comments` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_lab_contributors`
--

DROP TABLE IF EXISTS `hcat_lab_contributors`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_lab_contributors` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `lab_id` int(11) NOT NULL,
  `contributor_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_lab_contributors_lab_id_contributor_id_5982dc54_uniq` (`lab_id`,`contributor_id`),
  KEY `hcat_lab_contributors_lab_id_d78ce88d` (`lab_id`),
  KEY `hcat_lab_contributors_contributor_id_99fd52fb` (`contributor_id`)
) ENGINE=MyISAM AUTO_INCREMENT=17 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_lab_contributors`
--

LOCK TABLES `hcat_lab_contributors` WRITE;
/*!40000 ALTER TABLE `hcat_lab_contributors` DISABLE KEYS */;
INSERT INTO `hcat_lab_contributors` VALUES (1,1,1),(2,1,2),(3,1,21),(4,1,22),(5,1,23),(6,2,24),(7,2,25),(8,3,4),(9,3,3),(10,3,5),(11,3,6),(12,3,7),(13,4,9),(14,4,10),(15,4,11),(16,4,12);
/*!40000 ALTER TABLE `hcat_lab_contributors` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_lab_grants`
--

DROP TABLE IF EXISTS `hcat_lab_grants`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_lab_grants` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `lab_id` int(11) NOT NULL,
  `grant_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_lab_grants_lab_id_grant_id_ece9905c_uniq` (`lab_id`,`grant_id`),
  KEY `hcat_lab_grants_lab_id_ac121b06` (`lab_id`),
  KEY `hcat_lab_grants_grant_id_0645f728` (`grant_id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_lab_grants`
--

LOCK TABLES `hcat_lab_grants` WRITE;
/*!40000 ALTER TABLE `hcat_lab_grants` DISABLE KEYS */;
/*!40000 ALTER TABLE `hcat_lab_grants` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_lab_projects`
--

DROP TABLE IF EXISTS `hcat_lab_projects`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_lab_projects` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `lab_id` int(11) NOT NULL,
  `project_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_lab_projects_lab_id_project_id_4c0352e1_uniq` (`lab_id`,`project_id`),
  KEY `hcat_lab_projects_lab_id_382d4513` (`lab_id`),
  KEY `hcat_lab_projects_project_id_71a5c491` (`project_id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_lab_projects`
--

LOCK TABLES `hcat_lab_projects` WRITE;
/*!40000 ALTER TABLE `hcat_lab_projects` DISABLE KEYS */;
/*!40000 ALTER TABLE `hcat_lab_projects` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_organ`
--

DROP TABLE IF EXISTS `hcat_organ`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_organ` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `short_name` varchar(50) NOT NULL,
  `description` varchar(250) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_organ_short_name_16091268_uniq` (`short_name`)
) ENGINE=MyISAM AUTO_INCREMENT=9 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_organ`
--

LOCK TABLES `hcat_organ` WRITE;
/*!40000 ALTER TABLE `hcat_organ` DISABLE KEYS */;
INSERT INTO `hcat_organ` VALUES (1,'tonsil','Immune organs near the throat'),(2,'lung','Lungs help with breathing, gas exchange, and blowing out birthday candles.'),(3,'full body','complete body used as specimen'),(4,'pancreas','Pancreas - source of insulin, digestive fluids, and other important secretions'),(5,'kidney','Kidney'),(6,'brain','The entire brain up to and including the stem but not the spinal cord'),(7,'liver','Connects your digestive system to the rest of your body and keeps you from bleeding'),(8,'umbilical cord','NEEDS DESCRIPTION');
/*!40000 ALTER TABLE `hcat_organ` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_organ_comments`
--

DROP TABLE IF EXISTS `hcat_organ_comments`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_organ_comments` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `organ_id` int(11) NOT NULL,
  `comment_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_organ_comments_organ_id_comme_fb59430f_uniq` (`organ_id`,`comment_id`),
  KEY `hcat_organ_comments_organ_id_c6b0ea98` (`organ_id`),
  KEY `hcat_organ_comments_comment_id_cd4392c6` (`comment_id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_organ_comments`
--

LOCK TABLES `hcat_organ_comments` WRITE;
/*!40000 ALTER TABLE `hcat_organ_comments` DISABLE KEYS */;
/*!40000 ALTER TABLE `hcat_organ_comments` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_organpart`
--

DROP TABLE IF EXISTS `hcat_organpart`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_organpart` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `short_name` varchar(50) NOT NULL,
  `description` varchar(250) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_organpart_short_name_06308530_uniq` (`short_name`)
) ENGINE=MyISAM AUTO_INCREMENT=8 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_organpart`
--

LOCK TABLES `hcat_organpart` WRITE;
/*!40000 ALTER TABLE `hcat_organpart` DISABLE KEYS */;
INSERT INTO `hcat_organpart` VALUES (1,'healthy tonsils from apnea patients','Healthy tonsil surgically removed from sleep apnea patients'),(2,'lung airway epithelium','cells harvested from the epithelium of lungs'),(4,'kidney cortex','Cortex of Kidney'),(5,'whole','complete organ was sampled'),(6,'kidney organoid','An organoid grown according to a kidney differentiation protocol'),(7,'cord blood','NEEDS DESCRIPTION');
/*!40000 ALTER TABLE `hcat_organpart` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_organpart_comments`
--

DROP TABLE IF EXISTS `hcat_organpart_comments`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_organpart_comments` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `organpart_id` int(11) NOT NULL,
  `comment_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_organpart_comm_organpart_id_c_49de483a_uniq` (`organpart_id`,`comment_id`),
  KEY `hcat_organpart_comments_organpart_id_1d67950e` (`organpart_id`),
  KEY `hcat_organpart_comments_comment_id_23a778f1` (`comment_id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_organpart_comments`
--

LOCK TABLES `hcat_organpart_comments` WRITE;
/*!40000 ALTER TABLE `hcat_organpart_comments` DISABLE KEYS */;
/*!40000 ALTER TABLE `hcat_organpart_comments` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_organpart_projects`
--

DROP TABLE IF EXISTS `hcat_organpart_projects`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_organpart_projects` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `organpart_id` int(11) NOT NULL,
  `project_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_organpart_projects_organpart_id_project_id_9ccf00df_uniq` (`organpart_id`,`project_id`),
  KEY `hcat_organpart_projects_organpart_id_845b3850` (`organpart_id`),
  KEY `hcat_organpart_projects_project_id_cd86deaf` (`project_id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_organpart_projects`
--

LOCK TABLES `hcat_organpart_projects` WRITE;
/*!40000 ALTER TABLE `hcat_organpart_projects` DISABLE KEYS */;
/*!40000 ALTER TABLE `hcat_organpart_projects` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_project`
--

DROP TABLE IF EXISTS `hcat_project`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_project` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `short_name` varchar(80) NOT NULL,
  `title` varchar(120) NOT NULL,
  `description` longtext NOT NULL,
  `wrangler1_id` int(11) DEFAULT NULL,
  `wrangler2_id` int(11) DEFAULT NULL,
  `stars` int(11) NOT NULL,
  `consent_id` int(11) DEFAULT NULL,
  `cells_expected` int(11) NOT NULL,
  `effort_id` int(11) DEFAULT NULL,
  `staging_area` varchar(200) DEFAULT NULL,
  `questionnaire` varchar(100) DEFAULT NULL,
  `sheet_curated` varchar(100) DEFAULT NULL,
  `sheet_from_lab` varchar(100) DEFAULT NULL,
  `sheet_template` varchar(100) DEFAULT NULL,
  `sheet_validated` varchar(100) DEFAULT NULL,
  `tAndC` varchar(100) DEFAULT NULL,
  `origin_name` varchar(200) NOT NULL,
  `first_contact_date` date DEFAULT NULL,
  `last_contact_date` date DEFAULT NULL,
  `first_response_date` date DEFAULT NULL,
  `last_response_date` date DEFAULT NULL,
  `questionnaire_date` date DEFAULT NULL,
  `sheet_curated_date` date DEFAULT NULL,
  `sheet_from_lab_date` date DEFAULT NULL,
  `sheet_template_date` date DEFAULT NULL,
  `sheet_validated_date` date DEFAULT NULL,
  `staging_area_date` date DEFAULT NULL,
  `tAndC_date` date DEFAULT NULL,
  `cloud_date` date DEFAULT NULL,
  `orange_date` date DEFAULT NULL,
  `pipeline_date` date DEFAULT NULL,
  `submit_date` date DEFAULT NULL,
  `cur_state_id` int(11) DEFAULT NULL,
  `state_reached_id` int(11) DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `hcat_project_wrangler_id_b0fa8cdf` (`wrangler1_id`),
  KEY `hcat_project_wrangler2_id_6a9604b0` (`wrangler2_id`),
  KEY `hcat_project_concent_id_fbd4fd2f` (`consent_id`),
  KEY `hcat_project_origin_id_bc12bf09` (`effort_id`),
  KEY `hcat_project_cur_state_id_39597c00` (`cur_state_id`),
  KEY `hcat_project_state_reached_id_a2e57236` (`state_reached_id`)
) ENGINE=MyISAM AUTO_INCREMENT=10 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_project`
--

LOCK TABLES `hcat_project` WRITE;
/*!40000 ALTER TABLE `hcat_project` DISABLE KEYS */;
INSERT INTO `hcat_project` VALUES (1,'HumanInnateLymphoidCells','Single cell RNA-sequencing of human tonsil Innate lymphoid cells (ILCs)','Single cell RNA-sequencing of human tonsil Innate lymphoid cells (ILCs) from three independent tonsil donors.',5,7,5,1,50000,1,NULL,'','','uploads/project/GSE70580.xlsx','','','','GSE70580',NULL,NULL,NULL,NULL,NULL,NULL,'2019-08-31',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,6,6),(2,'TabulaMuris','A single cell atlas of the mouse','This is a large study combining several single cell sequencing techniques run out of the biohub.',7,NULL,4,2,1000000,2,NULL,'','','','','','','Tabula Muris',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,1,7),(3,'RevisedAirwayEpithelialHierarchy','A revised airway epithelial hierarchy includes CFTR-expressing ionocytes','Airways conduct gases to the lung and are disease sites of asthma and cystic fibrosis. Here we study the cellular composition and hierarchy of the mouse tracheal epithelium by single-cell RNA-sequencing (scRNA-seq) and in vivo lineage tracing. We identify a rare cell type, the Foxi1+ pulmonary ionocyte; functional variations in club cells by proximodistal location; a distinct cell type in high turnover squamous epithelial structures that we term \'hillocks\'; and disease-relevant subsets of tuft and goblet cells. We developed \'pulse-seq\' , combining scRNA-seq and lineage tracing, to show that tuft, neuroendocrine and ionocyte cells are continually and directly replenished by basal progenitor cells. Ionocytes are the major source of transcripts of the cystic fibrosis transmembrane conductance regulator in both mouse (Cftr) and human (CFTR). Knockout of Foxi1 in mouse ionocytes causes loss of Cftr expression and disrupts airway fluid and mucus physiology, phenotypes that characterize cystic fibrosis. By associating cell-type-specific expression programs with key disease genes, we establish a new cellular narrative for airways disease.',5,7,4,2,0,1,NULL,'','','uploads/project/GSE103354_s8tqNaC.xlsx','','','','GSE103354',NULL,NULL,NULL,NULL,NULL,NULL,'2019-08-31',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,4,4),(5,'humphreysKidneyOrganoids1','Ben Humphreys Kidney Organoid snRNA-seq','Single nuclei rna-seq was performed on kidney organoids...',5,NULL,3,NULL,0,2,NULL,'','','','','','','',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,4,4),(9,'HumanLymphoMyeloidProgenitorCells','Single Cell RNA sequencing analysis of human haemopoietic lympho-myeloid progenitor populations (LMPP, MLP and GMP)','We performed single cell RNA sequencing of human haemopoietic lympho-myeloid progenitors with the aim to separate functionally and transcriptionally distinct progenitors within the heterogeneous GMP, LMPP and MLP populations. We performed RNA sequencing in a total of 415 single cells. From donors 1 (CB369) and 2 (CB431), 163/166 and 157/249 cells passed quality control.We further analysed the 320 single cells that passed quality control from two  different donors (157 and 163 from each donor; 91 LMPP, 110 MLP and 119 GMP). We performed clustering using Seurat package, which identified 3 clusters of cells.  We identified genes that were differentially expressed among cells of the different clusters. The majority of the cells in cluster 1 were MLP, the majority of cluster 3 were GMP while cluster comprised of LMPP and GMP cells. Cluster 1 showed high expression of lymphoid-affiliated genes. Conversely, cluster 3 showed increased expression of myeloid genes, while cluster 2 showed a mixed transcriptional signature. PCA analysis revealed a transcriptional continuum of LMPP, MLP and GMP populations.',NULL,NULL,5,1,0,1,NULL,NULL,NULL,'/uploads/project/GSE100618.tsv',NULL,NULL,NULL,'GSE100618',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,1,1);
/*!40000 ALTER TABLE `hcat_project` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_project_assay_tech`
--

DROP TABLE IF EXISTS `hcat_project_assay_tech`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_project_assay_tech` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `project_id` int(11) NOT NULL,
  `assaytech_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_project_assay_tech_project_id_assaytech_id_099970cc_uniq` (`project_id`,`assaytech_id`),
  KEY `hcat_project_assay_tech_project_id_0d1877f7` (`project_id`),
  KEY `hcat_project_assay_tech_assaytech_id_1a961e03` (`assaytech_id`)
) ENGINE=MyISAM AUTO_INCREMENT=6 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_project_assay_tech`
--

LOCK TABLES `hcat_project_assay_tech` WRITE;
/*!40000 ALTER TABLE `hcat_project_assay_tech` DISABLE KEYS */;
INSERT INTO `hcat_project_assay_tech` VALUES (1,2,1),(2,1,1),(5,2,2);
/*!40000 ALTER TABLE `hcat_project_assay_tech` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_project_assay_type`
--

DROP TABLE IF EXISTS `hcat_project_assay_type`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_project_assay_type` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `project_id` int(11) NOT NULL,
  `assaytype_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_project_assay_type_project_id_assaytype_id_4ebafd00_uniq` (`project_id`,`assaytype_id`),
  KEY `hcat_project_assay_type_project_id_843e2499` (`project_id`),
  KEY `hcat_project_assay_type_assaytype_id_2d24e846` (`assaytype_id`)
) ENGINE=MyISAM AUTO_INCREMENT=10 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_project_assay_type`
--

LOCK TABLES `hcat_project_assay_type` WRITE;
/*!40000 ALTER TABLE `hcat_project_assay_type` DISABLE KEYS */;
INSERT INTO `hcat_project_assay_type` VALUES (1,2,1),(2,1,1),(3,5,1),(7,3,1),(8,3,5),(9,9,1);
/*!40000 ALTER TABLE `hcat_project_assay_type` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_project_comments`
--

DROP TABLE IF EXISTS `hcat_project_comments`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_project_comments` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `project_id` int(11) NOT NULL,
  `comment_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_project_comments_project_id_comment_id_c010100e_uniq` (`project_id`,`comment_id`),
  KEY `hcat_project_comments_project_id_d7d02fff` (`project_id`),
  KEY `hcat_project_comments_comment_id_663fdf36` (`comment_id`)
) ENGINE=MyISAM AUTO_INCREMENT=6 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_project_comments`
--

LOCK TABLES `hcat_project_comments` WRITE;
/*!40000 ALTER TABLE `hcat_project_comments` DISABLE KEYS */;
INSERT INTO `hcat_project_comments` VALUES (2,5,5),(5,5,19),(4,2,18);
/*!40000 ALTER TABLE `hcat_project_comments` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_project_contacts`
--

DROP TABLE IF EXISTS `hcat_project_contacts`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_project_contacts` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `project_id` int(11) NOT NULL,
  `contributor_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_project_contacts_project_id_contributor_id_5ba2931c_uniq` (`project_id`,`contributor_id`),
  KEY `hcat_project_contacts_project_id_ce077dfa` (`project_id`),
  KEY `hcat_project_contacts_contributor_id_960b1526` (`contributor_id`)
) ENGINE=MyISAM AUTO_INCREMENT=2 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_project_contacts`
--

LOCK TABLES `hcat_project_contacts` WRITE;
/*!40000 ALTER TABLE `hcat_project_contacts` DISABLE KEYS */;
/*!40000 ALTER TABLE `hcat_project_contacts` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_project_contributors`
--

DROP TABLE IF EXISTS `hcat_project_contributors`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_project_contributors` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `project_id` int(11) NOT NULL,
  `contributor_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_project_contributor_project_id_contributor_i_b55c023b_uniq` (`project_id`,`contributor_id`),
  KEY `hcat_project_contributors_project_id_7c4f2183` (`project_id`),
  KEY `hcat_project_contributors_contributor_id_678dfbc4` (`contributor_id`)
) ENGINE=MyISAM AUTO_INCREMENT=58 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_project_contributors`
--

LOCK TABLES `hcat_project_contributors` WRITE;
/*!40000 ALTER TABLE `hcat_project_contributors` DISABLE KEYS */;
INSERT INTO `hcat_project_contributors` VALUES (1,1,1),(2,1,4),(3,1,2),(4,1,21),(5,1,22),(6,1,23),(7,2,24),(8,2,25),(9,2,7),(10,3,26),(11,3,27),(12,3,28),(13,3,29),(14,3,30),(15,3,31),(16,3,32),(17,3,33),(18,3,34),(19,3,35),(20,3,37),(21,3,38),(22,3,39),(23,3,40),(24,3,41),(25,3,42),(26,3,43),(27,3,44),(28,3,45),(29,3,46),(30,3,47),(31,3,48),(32,3,49),(33,3,50),(34,3,51),(35,3,52),(36,3,53),(37,3,54),(38,3,55),(53,5,4),(54,9,9),(55,9,10),(56,9,11),(57,9,12);
/*!40000 ALTER TABLE `hcat_project_contributors` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_project_disease`
--

DROP TABLE IF EXISTS `hcat_project_disease`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_project_disease` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `project_id` int(11) NOT NULL,
  `disease_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_project_disease_project_id_disease_id_59397fe9_uniq` (`project_id`,`disease_id`),
  KEY `hcat_project_disease_project_id_9dab5ae6` (`project_id`),
  KEY `hcat_project_disease_disease_id_5631dfbf` (`disease_id`)
) ENGINE=MyISAM AUTO_INCREMENT=10 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_project_disease`
--

LOCK TABLES `hcat_project_disease` WRITE;
/*!40000 ALTER TABLE `hcat_project_disease` DISABLE KEYS */;
INSERT INTO `hcat_project_disease` VALUES (1,1,1),(2,2,2),(3,3,2),(9,9,7);
/*!40000 ALTER TABLE `hcat_project_disease` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_project_files`
--

DROP TABLE IF EXISTS `hcat_project_files`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_project_files` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `project_id` int(11) NOT NULL,
  `file_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_project_files_project_id_file_id_5e416d7c_uniq` (`project_id`,`file_id`),
  KEY `hcat_project_files_project_id_1ed539e0` (`project_id`),
  KEY `hcat_project_files_file_id_5351ee7a` (`file_id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_project_files`
--

LOCK TABLES `hcat_project_files` WRITE;
/*!40000 ALTER TABLE `hcat_project_files` DISABLE KEYS */;
/*!40000 ALTER TABLE `hcat_project_files` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_project_grants`
--

DROP TABLE IF EXISTS `hcat_project_grants`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_project_grants` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `project_id` int(11) NOT NULL,
  `grant_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_project_grants_project_id_grant_id_85770e16_uniq` (`project_id`,`grant_id`),
  KEY `hcat_project_grants_project_id_44cf85bc` (`project_id`),
  KEY `hcat_project_grants_grant_id_9cbff215` (`grant_id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_project_grants`
--

LOCK TABLES `hcat_project_grants` WRITE;
/*!40000 ALTER TABLE `hcat_project_grants` DISABLE KEYS */;
/*!40000 ALTER TABLE `hcat_project_grants` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_project_labs`
--

DROP TABLE IF EXISTS `hcat_project_labs`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_project_labs` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `project_id` int(11) NOT NULL,
  `lab_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_project_labs_project_id_lab_id_841c4833_uniq` (`project_id`,`lab_id`),
  KEY `hcat_project_labs_project_id_dfc8abdd` (`project_id`),
  KEY `hcat_project_labs_lab_id_7261c9d2` (`lab_id`)
) ENGINE=MyISAM AUTO_INCREMENT=6 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_project_labs`
--

LOCK TABLES `hcat_project_labs` WRITE;
/*!40000 ALTER TABLE `hcat_project_labs` DISABLE KEYS */;
INSERT INTO `hcat_project_labs` VALUES (1,1,1),(2,2,2),(5,9,4);
/*!40000 ALTER TABLE `hcat_project_labs` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_project_organ`
--

DROP TABLE IF EXISTS `hcat_project_organ`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_project_organ` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `project_id` int(11) NOT NULL,
  `organ_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_project_organ_project_id_organ_id_c5884702_uniq` (`project_id`,`organ_id`),
  KEY `hcat_project_organ_project_id_a275bb67` (`project_id`),
  KEY `hcat_project_organ_organ_id_a1ca29e9` (`organ_id`)
) ENGINE=MyISAM AUTO_INCREMENT=4 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_project_organ`
--

LOCK TABLES `hcat_project_organ` WRITE;
/*!40000 ALTER TABLE `hcat_project_organ` DISABLE KEYS */;
INSERT INTO `hcat_project_organ` VALUES (2,1,1),(3,9,8);
/*!40000 ALTER TABLE `hcat_project_organ` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_project_organ_part`
--

DROP TABLE IF EXISTS `hcat_project_organ_part`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_project_organ_part` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `project_id` int(11) NOT NULL,
  `organpart_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_project_body_part_project_id_bodypart_id_2181d641_uniq` (`project_id`,`organpart_id`),
  KEY `hcat_project_body_part_project_id_deafaf9e` (`project_id`),
  KEY `hcat_project_body_part_bodypart_id_a4860a5d` (`organpart_id`)
) ENGINE=MyISAM AUTO_INCREMENT=10 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_project_organ_part`
--

LOCK TABLES `hcat_project_organ_part` WRITE;
/*!40000 ALTER TABLE `hcat_project_organ_part` DISABLE KEYS */;
INSERT INTO `hcat_project_organ_part` VALUES (1,1,1),(2,3,2),(7,5,6),(9,9,7);
/*!40000 ALTER TABLE `hcat_project_organ_part` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_project_publications`
--

DROP TABLE IF EXISTS `hcat_project_publications`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_project_publications` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `project_id` int(11) NOT NULL,
  `publication_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_project_publication_project_id_publication_i_8c90a466_uniq` (`project_id`,`publication_id`),
  KEY `hcat_project_publications_project_id_95d1e789` (`project_id`),
  KEY `hcat_project_publications_publication_id_313b37c7` (`publication_id`)
) ENGINE=MyISAM AUTO_INCREMENT=4 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_project_publications`
--

LOCK TABLES `hcat_project_publications` WRITE;
/*!40000 ALTER TABLE `hcat_project_publications` DISABLE KEYS */;
INSERT INTO `hcat_project_publications` VALUES (1,1,1),(2,3,2),(3,9,3);
/*!40000 ALTER TABLE `hcat_project_publications` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_project_responders`
--

DROP TABLE IF EXISTS `hcat_project_responders`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_project_responders` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `project_id` int(11) NOT NULL,
  `contributor_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_project_responders_project_id_contributor_id_5fac808b_uniq` (`project_id`,`contributor_id`),
  KEY `hcat_project_responders_project_id_25c477ea` (`project_id`),
  KEY `hcat_project_responders_contributor_id_9e28731c` (`contributor_id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_project_responders`
--

LOCK TABLES `hcat_project_responders` WRITE;
/*!40000 ALTER TABLE `hcat_project_responders` DISABLE KEYS */;
/*!40000 ALTER TABLE `hcat_project_responders` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_project_sample_type`
--

DROP TABLE IF EXISTS `hcat_project_sample_type`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_project_sample_type` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `project_id` int(11) NOT NULL,
  `sampletype_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_project_sample_type_project_id_sampletype_id_24d5e6dc_uniq` (`project_id`,`sampletype_id`),
  KEY `hcat_project_sample_type_project_id_219fb241` (`project_id`),
  KEY `hcat_project_sample_type_sampletype_id_d697a9eb` (`sampletype_id`)
) ENGINE=MyISAM AUTO_INCREMENT=9 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_project_sample_type`
--

LOCK TABLES `hcat_project_sample_type` WRITE;
/*!40000 ALTER TABLE `hcat_project_sample_type` DISABLE KEYS */;
INSERT INTO `hcat_project_sample_type` VALUES (2,5,4),(5,1,1);
/*!40000 ALTER TABLE `hcat_project_sample_type` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_project_species`
--

DROP TABLE IF EXISTS `hcat_project_species`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_project_species` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `project_id` int(11) NOT NULL,
  `species_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_project_species_project_id_species_id_cf70c5cc_uniq` (`project_id`,`species_id`),
  KEY `hcat_project_species_project_id_99f8f1ab` (`project_id`),
  KEY `hcat_project_species_species_id_7a05e848` (`species_id`)
) ENGINE=MyISAM AUTO_INCREMENT=11 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_project_species`
--

LOCK TABLES `hcat_project_species` WRITE;
/*!40000 ALTER TABLE `hcat_project_species` DISABLE KEYS */;
INSERT INTO `hcat_project_species` VALUES (1,1,1),(2,2,2),(5,3,2),(6,5,1),(10,9,1);
/*!40000 ALTER TABLE `hcat_project_species` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_project_submit_comments`
--

DROP TABLE IF EXISTS `hcat_project_submit_comments`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_project_submit_comments` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `project_id` int(11) NOT NULL,
  `comment_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_project_submit_comments_project_id_comment_id_17013298_uniq` (`project_id`,`comment_id`),
  KEY `hcat_project_submit_comments_project_id_17a2be84` (`project_id`),
  KEY `hcat_project_submit_comments_comment_id_00b915af` (`comment_id`)
) ENGINE=MyISAM AUTO_INCREMENT=2 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_project_submit_comments`
--

LOCK TABLES `hcat_project_submit_comments` WRITE;
/*!40000 ALTER TABLE `hcat_project_submit_comments` DISABLE KEYS */;
INSERT INTO `hcat_project_submit_comments` VALUES (1,2,9);
/*!40000 ALTER TABLE `hcat_project_submit_comments` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_project_urls`
--

DROP TABLE IF EXISTS `hcat_project_urls`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_project_urls` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `project_id` int(11) NOT NULL,
  `url_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_project_urls_project_id_url_id_24c08cd7_uniq` (`project_id`,`url_id`),
  KEY `hcat_project_urls_project_id_dee701af` (`project_id`),
  KEY `hcat_project_urls_url_id_983d2564` (`url_id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_project_urls`
--

LOCK TABLES `hcat_project_urls` WRITE;
/*!40000 ALTER TABLE `hcat_project_urls` DISABLE KEYS */;
/*!40000 ALTER TABLE `hcat_project_urls` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_projectstate`
--

DROP TABLE IF EXISTS `hcat_projectstate`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_projectstate` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `state` varchar(30) NOT NULL,
  `description` varchar(100) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_projectstate_state_d27d4743_uniq` (`state`)
) ENGINE=MyISAM AUTO_INCREMENT=14 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_projectstate`
--

LOCK TABLES `hcat_projectstate` WRITE;
/*!40000 ALTER TABLE `hcat_projectstate` DISABLE KEYS */;
INSERT INTO `hcat_projectstate` VALUES (1,'suitable','Interesting dataset might be good for HCA'),(2,'unsuitable','Project evaluated and found unsuitable for the HCA'),(3,'stalled','Work got started but it\'s stalled until somebody kicks it'),(4,'wrangling','Wrangling work is proceeding actively on project'),(5,'review','Initial wrangling done, being reviewed by quality assurance  and lab'),(6,'staged','wrangler and lab have signed project ready to submit and files in staging area'),(7,'submitted','Project has been submitted to ingest'),(8,'ingested','project has passed through ingestion and is visible in blue box'),(9,'primary analysis','stuff that needs to be before producing gene/cell matrix.  Aligning reads, registering images, etc.'),(10,'cell matrix','a matrix that relates cell to cell measurements, typically RNA levels'),(11,'cell clusters','Have a mechanically generated typology of cells inferred from the data'),(12,'cell types','Have a manually curated set of cell types relevant to this data set'),(13,'integrated analysis','work has been done integrating this with other HCA data sets');
/*!40000 ALTER TABLE `hcat_projectstate` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_projectstate_comments`
--

DROP TABLE IF EXISTS `hcat_projectstate_comments`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_projectstate_comments` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `projectstate_id` int(11) NOT NULL,
  `comment_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_projectstate_c_projectstate_id_voc_93194194_uniq` (`projectstate_id`,`comment_id`),
  KEY `hcat_projectstate_comments_projectstate_id_c5520db9` (`projectstate_id`),
  KEY `hcat_projectstate_comments_comment_id_6343cefb` (`comment_id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_projectstate_comments`
--

LOCK TABLES `hcat_projectstate_comments` WRITE;
/*!40000 ALTER TABLE `hcat_projectstate_comments` DISABLE KEYS */;
/*!40000 ALTER TABLE `hcat_projectstate_comments` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_publication`
--

DROP TABLE IF EXISTS `hcat_publication`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_publication` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `short_name` varchar(50) NOT NULL,
  `title` varchar(250) NOT NULL,
  `pmid` varchar(16) NOT NULL,
  `doi` varchar(32) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `short_name` (`short_name`)
) ENGINE=MyISAM AUTO_INCREMENT=4 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_publication`
--

LOCK TABLES `hcat_publication` WRITE;
/*!40000 ALTER TABLE `hcat_publication` DISABLE KEYS */;
INSERT INTO `hcat_publication` VALUES (1,'pmid: 26878113','The heterogeneity of human CD127(+) innate lymphoid cells revealed by single-cell RNA sequencing.','26878113','10.1038/ni.3368'),(2,'pmid: 30069044','A revised airway epithelial hierarchy includes CFTR-expressing ionocytes.','30069044','10.1038/s41586-018-0393-7'),(3,'pmid: 29167569','','29167569','');
/*!40000 ALTER TABLE `hcat_publication` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_publication_comments`
--

DROP TABLE IF EXISTS `hcat_publication_comments`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_publication_comments` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `publication_id` int(11) NOT NULL,
  `comment_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_publication_co_publication_id_voca_f53bfe4e_uniq` (`publication_id`,`comment_id`),
  KEY `hcat_publication_comments_publication_id_0506fab0` (`publication_id`),
  KEY `hcat_publication_comments_comment_id_1a7dfd64` (`comment_id`)
) ENGINE=MyISAM AUTO_INCREMENT=2 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_publication_comments`
--

LOCK TABLES `hcat_publication_comments` WRITE;
/*!40000 ALTER TABLE `hcat_publication_comments` DISABLE KEYS */;
INSERT INTO `hcat_publication_comments` VALUES (1,2,15);
/*!40000 ALTER TABLE `hcat_publication_comments` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_sampletype`
--

DROP TABLE IF EXISTS `hcat_sampletype`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_sampletype` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `short_name` varchar(50) NOT NULL,
  `description` varchar(250) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_sampletype_short_name_0d95d0a4_uniq` (`short_name`)
) ENGINE=MyISAM AUTO_INCREMENT=9 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_sampletype`
--

LOCK TABLES `hcat_sampletype` WRITE;
/*!40000 ALTER TABLE `hcat_sampletype` DISABLE KEYS */;
INSERT INTO `hcat_sampletype` VALUES (1,'fresh sample','Sample not frozen, cultured, or preserved and used in a timely manner while still alive and healthy'),(2,'standard cell line','a standard cell line that can be purchased from commercial sources easiliy'),(3,'iPSC','Induced pluripotent stem cell'),(4,'organoid','organoid artificially produced from stem cells'),(5,'cancer','cancerous cells'),(6,'frozen sample','sample frozen while fresh and stored suitably'),(7,'primary cell line','cells cultured from fresh tissue samples without treatment to immortalize the culture'),(8,'immortalized cell line','cells treated with EBV or other agents so that they can divide forever, escaping the Hayflick limit');
/*!40000 ALTER TABLE `hcat_sampletype` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_sampletype_comments`
--

DROP TABLE IF EXISTS `hcat_sampletype_comments`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_sampletype_comments` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `sampletype_id` int(11) NOT NULL,
  `comment_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_sampletype_com_sampletype_id__f2ca88a9_uniq` (`sampletype_id`,`comment_id`),
  KEY `hcat_sampletype_comments_sampletype_id_cad69b1b` (`sampletype_id`),
  KEY `hcat_sampletype_comments_comment_id_fa72ddd6` (`comment_id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_sampletype_comments`
--

LOCK TABLES `hcat_sampletype_comments` WRITE;
/*!40000 ALTER TABLE `hcat_sampletype_comments` DISABLE KEYS */;
/*!40000 ALTER TABLE `hcat_sampletype_comments` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_species`
--

DROP TABLE IF EXISTS `hcat_species`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_species` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `common_name` varchar(40) NOT NULL,
  `scientific_name` varchar(150) NOT NULL,
  `ncbi_taxon` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_species_ncbi_taxon_04279782_uniq` (`ncbi_taxon`)
) ENGINE=MyISAM AUTO_INCREMENT=3 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_species`
--

LOCK TABLES `hcat_species` WRITE;
/*!40000 ALTER TABLE `hcat_species` DISABLE KEYS */;
INSERT INTO `hcat_species` VALUES (1,'human','Homo sapiens',9606),(2,'mouse','Mus musculus',10090);
/*!40000 ALTER TABLE `hcat_species` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_url`
--

DROP TABLE IF EXISTS `hcat_url`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_url` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `short_name` varchar(50) NOT NULL,
  `path` varchar(200) NOT NULL,
  `description` varchar(255) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `short_name` (`short_name`)
) ENGINE=MyISAM AUTO_INCREMENT=2 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_url`
--

LOCK TABLES `hcat_url` WRITE;
/*!40000 ALTER TABLE `hcat_url` DISABLE KEYS */;
INSERT INTO `hcat_url` VALUES (1,'UCSC Genome Browser','https://genome.ucsc.edu','Many different annotations mapped to the genomic sequence viewable at a variety of scales.');
/*!40000 ALTER TABLE `hcat_url` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hcat_url_comments`
--

DROP TABLE IF EXISTS `hcat_url_comments`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `hcat_url_comments` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `url_id` int(11) NOT NULL,
  `comment_id` int(11) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `hcat_url_comments_url_id_comment_id_972fb338_uniq` (`url_id`,`comment_id`),
  KEY `hcat_url_comments_url_id_d4008533` (`url_id`),
  KEY `hcat_url_comments_comment_id_e9d22771` (`comment_id`)
) ENGINE=MyISAM AUTO_INCREMENT=2 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hcat_url_comments`
--

LOCK TABLES `hcat_url_comments` WRITE;
/*!40000 ALTER TABLE `hcat_url_comments` DISABLE KEYS */;
INSERT INTO `hcat_url_comments` VALUES (1,1,9);
/*!40000 ALTER TABLE `hcat_url_comments` ENABLE KEYS */;
UNLOCK TABLES;
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

