#Text to Genome project marker bed: bed4 plus one additional field for count
CREATE TABLE `pubsMarkerSnp` (
  `chrom` varchar(255) NOT NULL, # chromosome
  `chromStart` int(10) unsigned NOT NULL, # start position on chromosome
  `chromEnd` int(10) unsigned NOT NULL, # end position on chromosome
  `name` varchar(255) NOT NULL, #name of marker, e.g. gene like TP53 or rs12345
  `matchCount` int(10), # number of articles that contain matches for this marker
  KEY `name` (`name`(16)),
  KEY `chrom` (`chrom`(6)),
  KEY `chromStartAndName` (`chrom`(6), chromStart, name)
);
