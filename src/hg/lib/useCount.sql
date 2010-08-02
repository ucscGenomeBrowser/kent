# usage counter for CGI useCount
CREATE TABLE useCount (
    count int not null auto_increment,	# auto-increment counter
    userAgent varchar(255) not null,	# HTTP_USER_AGENT
    remoteAddr varchar(255) not null,	# REMOTE_ADDR
    dateTime datetime not null,		# YYYY-MM-DD HH:MM:SS access time
    version varchar(255) not null,	# string in argument version=xxx
              #Indices
    PRIMARY KEY(count)
);
