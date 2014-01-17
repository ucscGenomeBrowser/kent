#publications blat feature table, in bed12+ format, additional field seqIds and seqRanges
CREATE TABLE `pubsBlat` (
  `chrom` varchar(255) NOT NULL, # chromosome
  `chromStart` int(10) unsigned NOT NULL, # start position on chromosome
  `chromEnd` int(10) unsigned NOT NULL, # end position on chromosome
  `name` varchar(255) NOT NULL, # internal articleId, article that matches here
  `score` int(10) unsigned NOT NULL, # score of feature
  `strand` char(1) NOT NULL, # strand of feature
  `thickStart` int(10) unsigned NOT NULL, # start of exons
  `thickEnd` int(10) unsigned NOT NULL, #end of exons
  `reserved` int(10) unsigned NOT NULL, # no clue
  `blockCount` int(10) unsigned NOT NULL, # number of blocks
  `blockSizes` longblob NOT NULL, # size of blocks
  `chromStarts` longblob NOT NULL, # A comma-separated list of block starts
  `tSeqTypes` varchar(255) NOT NULL, # comma-seq list of matching sequence db (g=genome, p=protein, c=cDNA)
  `seqIds` blob NOT NULL, # comma-separated list of matching seqIds
  `seqRanges` blob NOT NULL, # ranges start-end on sequence that matched, one for each seqId 
  `publisher` varchar(255) NOT NULL, # publisher of article, for hgTracks feature filter
  `pmid` varchar(255) NOT NULL, # PMID of article, for annoGrator output, avoids table join
  `doi` varchar(255) NOT NULL, # doi of article, for annoGrator output, avoids table join
  `issn` varchar(255) NOT NULL, # issn of journal 
  `journal` varchar(255) NOT NULL, # name of journal
  `title` varchar(255) NOT NULL, # title of article, for genome browser mouseover
  `firstAuthor` varchar(255) NOT NULL, # first author family name of article, for genome browser
  `year` varchar(255) NOT NULL, # year of article, for genome browser
  `impact` varchar(255) NOT NULL, # impact factor of journal, for genome browser coloring, derived from official impact factors: max impact is 25, value is scaled to 0-255
  `classes` varchar(255) NOT NULL, # classes assigned to journal article, for genome browser coloring
  `locus` varchar(255) NOT NULL, # closest gene symbols, one or two, comma-separated
  KEY `name` (`name`(16)),
  KEY `pmid` (`pmid`(16)),
  KEY `doi` (`doi`(16)),
  KEY `chrom` (`chrom`(6)),
  KEY `chromStartAndName` (`chrom`(6), chromStart, name)
);
