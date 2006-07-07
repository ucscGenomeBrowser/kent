
CREATE TABLE gap (
   bin smallint not null,
   chrom varchar(255) not null,
   chromStart int unsigned not null,
   chromEnd int unsigned not null,
   ix int not null,	
   n char(1) not null,
   size int unsigned not null,	
   type varchar(255) not null,
   bridge varchar(255) not null,
   INDEX(bin)
);

