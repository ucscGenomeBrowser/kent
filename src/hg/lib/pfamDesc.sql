CREATE TABLE pfamDesc (
  pfamAC          varchar(40) NOT NULL,
  pfamID          varchar(255) NOT NULL,
  description     varchar(255),
  KEY  (pfamID),
  KEY  (pfamAC)
) TYPE=MyISAM;

