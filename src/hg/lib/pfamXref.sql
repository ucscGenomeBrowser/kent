CREATE TABLE pfamXref (
  pfamAC         varchar(40) NOT NULL,
  swissAC        varchar(40),
  swissDisplayID varchar(40),
  KEY  (pfamAC),
  KEY  (swissAC),
  KEY  (swissDisplayID)
) ENGINE=MyISAM;;

