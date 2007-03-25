CREATE TABLE knownToHprd (
name  varchar(255) NOT NULL default '',
value varchar(255) NOT NULL default '',
KEY name (name(16)),
INDEX value (value(16))
) TYPE=MyISAM;
