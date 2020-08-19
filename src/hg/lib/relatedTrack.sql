#The relatedTrack table describing reasons why two tracks are related
CREATE TABLE `relatedTrack` (
track1 varchar(255) DEFAULT NULL,
track2 varchar(255) DEFAULT NULL,
why varchar(4096) DEFAULT NULL,
KEY track1 (track1),
KEY track2 (track2)
) ENGINE=MyISAM DEFAULT CHARSET=latin1
