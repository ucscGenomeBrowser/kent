CREATE TABLE samSubdir (
  proteinId varchar(40) NOT NULL default '', # protein ID
  subdir varchar(255) default NULL,	     # subdirectory in whicht the data of this protein are in
  KEY proteinID (proteinID)
) TYPE=MyISAM;

