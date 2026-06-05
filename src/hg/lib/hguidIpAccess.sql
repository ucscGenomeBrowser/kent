# hguidIpAccess.sql creates a database table to store recent accesses from
# hguid/ip combinations.  The indices facilitate quick insert/update and
# lookup, as this table will see a lot of traffic.

CREATE TABLE hguidIpAccess (
    userId int unsigned not null,       # User ID, matching an entry in userDb
    ip varchar(45) not null,            # IP address this userId was seen from
    lastSeen datetime not null,         # Most recent access time for this userId/ip combo
            #Indices
    PRIMARY KEY (userId, ip),
    INDEX lastSeenIdx(lastSeen)
);
