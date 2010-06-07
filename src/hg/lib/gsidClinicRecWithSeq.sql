CREATE TABLE gsidClinicRecWithSeq (
  specimenId varchar(255) NULL,
  subjId varchar(255) NULL,
  labCode varchar(255) NULL,
  daysCollection int(11) NULL,
  hivQuan int(10) NULL,
  cd4Count int(10) NULL,
  KEY specimenId (specimenId),
  KEY subjId (subjId)
) TYPE=MyISAM;

