CREATE TABLE pdbSP (
  pdb varchar(40) NOT NULL default '',
  sp varchar(40) NOT NULL default '',
  KEY i1 (pdb),
  KEY i2 (sp)
) TYPE=MyISAM DATA DIRECTORY='./proteins060115/' INDEX DIRECTORY='./proteins060115/';

