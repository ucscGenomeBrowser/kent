CREATE TABLE gsidClinicRec (
  specimenId varchar(255) NOT NULL default '',
  subjId varchar(255) NOT NULL default '',
  labCode varchar(255) NOT NULL default '',
  daysCollection int(11) NOT NULL default '0',
  hivQuan int(10) NOT NULL default '0',
  cd4Count int(10) NOT NULL default '0',
  KEY specimenId (specimenId),
  KEY subjId (subjId)
) TYPE=MyISAM;

