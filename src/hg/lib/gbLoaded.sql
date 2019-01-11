
create table gbLoaded (
  srcDb enum('GenBank','RefSeq') not null,
  type enum('EST','mRNA') not null,
  loadRelease char(8) not null,
  loadUpdate char(10) not null,
  accPrefix char(2) not null,
  time timestamp not null,
  extFileUpdated tinyint(1) not null,
  index(srcDb,loadRelease));

