# MySQL dump 7.1
#
# Host: localhost    Database: hg5
#--------------------------------------------------------
# Server version	3.22.32

#
# Table structure for table 'snpNih'
#
CREATE TABLE snpNih (
  chrom varchar(255) DEFAULT '' NOT NULL,
  chromStart int(10) unsigned DEFAULT '0' NOT NULL,
  chromEnd int(10) unsigned DEFAULT '0' NOT NULL,
  name varchar(255) DEFAULT '' NOT NULL,
  KEY chrom (chrom(12),chromStart),
  KEY chrom_2 (chrom(12),chromEnd)
);
