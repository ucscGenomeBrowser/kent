CREATE TABLE protMrnaScore (
  protAcc varchar(40) NOT NULL default '',
  mrnaAcc varchar(40) NOT NULL default '',
  score int NOT NULL,
  KEY protAcc (protAcc)
) TYPE=MyISAM;

