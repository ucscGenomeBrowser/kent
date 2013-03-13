CREATE TABLE `cvDb_results` (
    `id` integer AUTO_INCREMENT NOT NULL PRIMARY KEY,
    `experiment` integer NOT NULL,
    `replicate` varchar(50) NOT NULL,
    `view` varchar(50) NOT NULL,
    `objType` varchar(50) NOT NULL,
    `fileName` varchar(255) NOT NULL,
    `md5sum` varchar(255) NOT NULL,
    `tableName` varchar(100) NOT NULL,
    `dateSubmitted` varchar(40) NOT NULL,
    `dateResubmitted` varchar(40) NOT NULL,
    `dateUnrestricted` varchar(40) NOT NULL
)
