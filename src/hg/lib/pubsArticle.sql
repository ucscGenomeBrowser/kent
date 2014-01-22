#publications track article metadata table
CREATE TABLE pubsArticle (
    articleId bigint not null,	# internal article ID, created during download
    extId varchar(2000) not null,	# publisher ID e.g. PMCxxxx or doi or sciencedirect ID or the URL in case of Bing/MSR
    pmid bigint,               # PubmedID if available
    doi varchar(255) ,               # DOI if available
    source varchar(255) not null,       # data source, e.g. elsevier, pmcftp or crawler
    publisher varchar(255) not null,     # publisher, e.g. npg or wiley
    citation varchar(2000) default null,	# source journal citation
    journal varchar(255), # name of journal
    eIssn varchar(255), # electronic issn of journal
    vol varchar(255), # volume of journal
    issue varchar(255), # issue of journal
    page varchar(255), # page or pagerange within issue
    year int not null,	# year of publication or 0 if not defined
    title varchar(6000) default null,	# article title
    authors varchar(6000) default null,	# author list for this article
    firstAuthor varchar(255) default null,	# first author family name
    abstract varchar(32000) not null,	# article abstract
    url varchar(1000) default null,	# url to fulltext of article
    dbs varchar(500) default null,      # list of DBs with matches to this article
              #Indices
    PRIMARY KEY(articleId),
    KEY extIdx(extId),
    KEY pmidIdx(pmid),
    KEY doiIdx(doi),
    FULLTEXT INDEX (citation, title, authors, abstract)
);
