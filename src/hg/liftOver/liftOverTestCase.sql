CREATE TABLE liftOverTestCase (
    id varchar(255) not null,		# test1.1, test1.2, etc.
    comment varchar(255) not null,	# description of test
    origAssembly varchar(255) not null, # original assembly
    newAssembly varchar(255) not null,  # new assembly
    origChrom varchar(255) not null,	# chrom in original assembly
    origStart int unsigned not null,	# start position in original assembly
    origEnd int unsigned not null,	# end position in original assembly
    status varchar(255) not null,	# SUCCESS or FAILURE
    message varchar(255) not null,	# message
    newChrom varchar(255),		# chrom in new assembly, can be null
    newStart int unsigned,		# start position in new assembly, can be null
    newEnd int unsigned 		# end position in new acembly, can be null
);


insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message) 
                         values ("test1.1", "initial", "hg10", "hg16", "chr1", 1, 1, "FAILURE", "Deleted in new");
insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message) 
                         values ("test1.2", "initial", "hg12", "hg13", "chr1", 1, 1, "FAILURE", "Deleted in new");
insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message) 
                         values ("test1.3", "initial", "hg12", "hg15", "chr1", 1, 1, "FAILURE", "Deleted in new");
insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message) 
                         values ("test1.4", "initial", "hg12", "hg16", "chr1", 1, 1, "FAILURE", "Deleted in new");
insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message) 
                         values ("test1.5", "initial", "hg13", "hg15", "chr1", 1, 1, "FAILURE", "Deleted in new");
insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message) 
                         values ("test1.6", "initial", "hg13", "hg16", "chr1", 1, 1, "FAILURE", "Deleted in new");
insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message) 
                         values ("test1.7", "initial", "hg15", "hg16", "chr1", 1, 1, "FAILURE", "Deleted in new");
insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message) 
                         values ("test1.8", "initial", "hg16", "hg17", "chr1", 1, 1, "FAILURE", "Deleted in new");

insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message) 
                         values ("test2.1", "full chrom", "hg10", "hg16", "chr1", 1, 250000000, "FAILURE", "Split in new");
insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message) 
                         values ("test2.2", "full chrom", "hg12", "hg13", "chr1", 1, 250000000, "FAILURE", "Split in new");
insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message) 
                         values ("test2.3", "full chrom", "hg12", "hg15", "chr1", 1, 250000000, "FAILURE", "Split in new");
insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message) 
                         values ("test2.4", "full chrom", "hg12", "hg16", "chr1", 1, 250000000, "FAILURE", "Split in new");
insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message) 
                         values ("test2.5", "full chrom", "hg13", "hg16", "chr1", 1, 250000000, "FAILURE", "Split in new");
insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message) 
                         values ("test2.6", "full chrom", "hg13", "hg15", "chr1", 1, 250000000, "FAILURE", "Split in new");
insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message) 
                         values ("test2.7", "full chrom", "hg15", "hg16", "chr1", 1, 250000000, "FAILURE", "Split in new");
insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message) 
                         values ("test2.8", "full chrom", "hg16", "hg17", "chr1", 1, 250000000, "FAILURE", "Split in new");

insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message, newChrom, newStart, newEnd) 
                         values ("test3.1", "contig NT_077402", "hg15", "hg16", "chr1", 50000, 217280, "SUCCESS", "", "chr1", 0, 167280);
insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message, newChrom, newStart, newEnd) 
                         values ("test3.2", "contig NT_032977", "hg15", "hg16", "chr1", 46908798, 68889433, "SUCCESS", "", "chr1", 46920182, 68901642);
insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message) 
                         values ("test3.3", "contig NT_034471", "hg13", "hg16", "chr1", 0, 535608, "FAILURE", "Split in new");
insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message) 
                         values ("test3.4", "contig NT_034471", "hg13", "hg15", "chr1", 0, 535608, "FAILURE", "Split in new");
insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message) 
                         values ("test3.5", "contig NT_077988", "hg16", "hg17", "chr1", 140794983, 140974431, "FAILURE", "Split in new");
insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message) 
                         values ("test3.6", "contig NT_022071", "hg16", "hg17", "chr1", 141024431, 141135329, "FAILURE", "Split in new");
insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message) 
                         values ("test3.7", "contig NT_077930", "hg16", "hg17", "chr1", 141185329, 141387038, "FAILURE", "Split in new");
insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message) 
                         values ("test3.8", "contig NT_077389", "hg16", "hg17", "chr1", 141537038, 141939137, "FAILURE", "Split in new");
insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message) 
                         values ("test3.8", "contig NT_077931", "hg16", "hg17", "chr1", 141989137, 142115614, "FAILURE", "Split in new");

insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message) 
                         values ("test4.1", "gap (telomere)", "hg15", "hg16", "chr1", 1, 50000, "FAILURE", "Deleted in new");
insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message) 
                         values ("test4.2", "gap (unbridged clone)", "hg15", "hg16", "chr1", 144819939, 145219939, "FAILURE", "Deleted in new");
insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message) 
                         values ("test4.3", "gap (bridged clone)", "hg15", "hg16", "chr1", 22709998, 22719998, "FAILURE", "Deleted in new");
insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message) 
                         values ("test4.4", "gap (centromere)", "hg15", "hg16", "chr1", 119894205, 122894205, "FAILURE", "Deleted in new");
insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message) 
                         values ("test4.5", "gap (heterochromatin)", "hg15", "hg16", "chr1", 122894205, 140894205, "FAILURE", "Deleted in new");

insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message, newChrom, newStart, newEnd) 
                         values ("test5.1", "repeat (simple)", "hg15", "hg16", "chr1", 50001, 50468, "SUCCESS", "", "chr1", 1, 468);
insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message, newChrom, newStart, newEnd) 
                         values ("test5.2", "repeat (satelite)", "hg15", "hg16", "chr1", 50469, 51310, "SUCCESS", "", "chr1", 469, 1310);
insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message, newChrom, newStart, newEnd) 
                         values ("test5.3", "repeat (SINE)", "hg15", "hg16", "chr1", 91448, 91743, "SUCCESS", "", "chr1", 41448, 41743);
insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message, newChrom, newStart, newEnd) 
                         values ("test5.4", "repeat (LINE)", "hg15", "hg16", "chr1", 88278, 89381, "SUCCESS", "", "chr1", 38278, 39381);
insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message, newChrom, newStart, newEnd) 
                         values ("test5.5", "repeat (LTR)", "hg15", "hg16", "chr1", 1267080, 1268046, "SUCCESS", "", "chr1", 1296589, 1297555);

insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message, newChrom, newStart, newEnd) 
                         values ("test6.1", "Refseq (NM_000014)", "hg15", "hg16", "chr12", 9120576, 9168754, "SUCCESS", "", "chr12", 9111576, 9159754);
insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message, newChrom, newStart, newEnd) 
                         values ("test6.2", "Refseq (NM_000015)", "hg15", "hg16", "chr8", 18058058, 18067986, "SUCCESS", "", "chr8", 18259027, 18268955);
insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message) 
                         values ("test6.3", "Refseq (NM_002122)", "hg15", "hg16", "chr6_random", 3908637, 3914439, "FAILURE", "Split in new");

insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message, newChrom, newStart, newEnd) 
                         values ("test7.1", "smaller random", "hg15", "hg16", "chr6_random", 1, 3800000, "SUCCESS", "", "chr6", 28804583, 32528981);
insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message) 
                         values ("test7.2", "smaller random", "hg15", "hg16", "chr6_random", 1, 3900000, "FAILURE", "Split in new");
insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message, newChrom, newStart, newEnd) 
                         values ("test7.3", "smaller random", "hg15", "hg16", "chr6_random", 4000000, 4200000, "SUCCESS", "", "chr6", 32743865, 32943511);
insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message, newChrom, newStart, newEnd) 
                         values ("test7.4", "smaller random", "hg15", "hg16", "chr6_random", 4290000, 9200000, "SUCCESS", "", "chr6", 28649262, 33398434);
insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message, newChrom, newStart, newEnd) 
                         values ("test7.5", "smaller random", "hg15", "hg16", "chr6_random", 9200000, 9220000, "SUCCESS", "", "chr5", 72035717, 72060021);
insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message) 
                         values ("test7.6", "deleted random", "hg15", "hg16", "chrY_random", 1, 191000, "FAILURE", "Deleted in new");
insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message, newChrom, newStart, newEnd) 
                         values ("test7.7", "deleted random", "hg15", "hg16", "chr11_random", 1, 2500, "SUCCESS", "", "chr11", 69379248, 69381748);

insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message, newChrom, newStart, newEnd) 
                         values ("test8.1", "1k", "hg15", "hg16", "chr1", 50001, 51000, "SUCCESS", "", "chr1", 1, 1000);
insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message, newChrom, newStart, newEnd) 
                         values ("test8.2", "10k", "hg15", "hg16", "chr1", 50001, 60000, "SUCCESS", "", "chr1", 1, 10000);

insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message, newChrom, newStart, newEnd) 
                         values ("test9.1", "long stable contig", "hg15", "hg16", "chr14", 18070001, 105261216, "SUCCESS", "", "chr14", 18070001, 105261216);
insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message, newChrom, newStart, newEnd) 
                         values ("test9.2", "long stable region", "hg16", "hg17", "chr14", 18000000, 105000000, "SUCCESS", "", "chr14", 19149712, 106099369);

insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message, newChrom, newStart, newEnd) 
                         values ("test10.1", "missing gap", "hg16", "hg17", "chr12", 17000, 20000, "SUCCESS", "", "chr12", 17000, 20000);
insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message, newChrom, newStart, newEnd) 
                         values ("test10.2", "missing gap", "hg16", "hg17", "chr12", 100000000, 100100000, "SUCCESS", "", "chr12", 100000000, 100100000);
insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message, newChrom, newStart, newEnd) 
                         values ("test10.3", "missing gap", "hg16", "hg17", "chr12", 120000000, 120100000, "SUCCESS", "", "chr12", 120050767, 120150767);
insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message, newChrom, newStart, newEnd) 
                         values ("test10.4", "missing gap", "hg16", "hg17", "chr12", 1, 133000000, "SUCCESS", "", "chr12", 16000, 132389811);

insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message, newChrom, newStart, newEnd) 
                         values ("test11.1", "longer initial short arm", "hg16", "hg17", "chr13", 16818001, 16820000, "SUCCESS", "", "chr13", 17918001, 17920000);

insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message, newChrom, newStart, newEnd) 
                         values ("test12.1", "SMN1 condensed", "hg16", "hg17", "chr5", 69077488, 69105554, "SUCCESS", "", "chr5", 70256523, 70284593);
insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message, newChrom, newStart, newEnd) 
                         values ("test12.2", "SMN1 condensed", "hg16", "hg17", "chr5", 70422996, 70451066, "SUCCESS", "", "chr5", 70256523, 70284593);
insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message, newChrom, newStart, newEnd) 
                         values ("test12.3", "BIRC1 retained", "hg16", "hg17", "chr5", 70466538, 70522752, "SUCCESS", "", "chr5", 70300065, 70356279);
insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message ) 
                         values ("test12.4", "BIRC1 removed", "hg16", "hg17", "chr5", 69005857, 69062049, "FAILURE", "Split in new");

insert into liftOverTestCase (id, comment, origAssembly, newAssembly, origChrom, origStart, origEnd, status, message, newChrom, newStart, newEnd) 
                         values ("test13.1", "move chroms", "hg16", "hg17", "chr1", 140358487, 140358535, "SUCCESS", "", "chr12", 53498115, 53498163);
