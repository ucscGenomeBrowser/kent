CREATE TABLE knownToTreefam (
name  varchar(255) NOT NULL default '',
value varchar(255) NOT NULL default '',
KEY name (name),
INDEX value (value)
) TYPE=MyISAM;
