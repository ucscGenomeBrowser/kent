select acc, commentType from uniProt.comment where commentType in (16, 21);
--use proteome;
--select accession, 'omimTitle' from spXref2 where division = '9606' and
--extDb = 'MIM';
--select refLink.mrnaAcc, 'omimTitle' from hg17.omimTitle, hg17.refLink where refLink.omimId = omimTitle.omimId;
