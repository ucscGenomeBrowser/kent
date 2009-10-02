CREATE TABLE webUsers (
  email varchar(255) NOT NULL default '',
  password varchar(255) NOT NULL default '',
  activated varchar(8) NOT NULL default '',
  name varchar(255) NOT NULL default '',
  phone varchar(255) NOT NULL default '',
  institution varchar(255) NOT NULL default '',
  registerDate varchar(255) NOT NULL default '',
  expireDate varchar(255) NOT NULL default '',
  PRIMARY KEY  (email)
) TYPE=MyISAM;

