DROP TABLE IF EXISTS test.chromInfo;

CREATE TABLE test.chromInfo (
  chrom varchar(255) NOT NULL default '',
  size int(10) unsigned NOT NULL default '0',
  fileName varchar(255) NOT NULL default ''
) ENGINE=MyISAM;

INSERT INTO test.chromInfo VALUES ('chrFake', 1000, '/dev/null');
INSERT INTO test.chromInfo VALUES ('chrFake_random', 100, '/dev/null');
