CREATE TABLE `rgdGeneXref2` (
  `rgdGeneId` varchar(40) NOT NULL,
  `extDB` varchar(40) NOT NULL,
  `extAC` varchar(40) NOT NULL,
  KEY `rgdGeneId` (`rgdGeneId`),
  KEY `extAC` (`extAC`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;
