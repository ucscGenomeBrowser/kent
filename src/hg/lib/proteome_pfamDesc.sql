CREATE TABLE pfamDesc (
  pfamAC varchar(40) NOT NULL default '',
  pfamID varchar(255) NOT NULL default '',
  description varchar(255) default NULL,
  KEY pfamID (pfamID),
  KEY pfamAC (pfamAC),
  KEY description (description(16))
) TYPE=MyISAM;

