table mysqlTableStatus
"Table status info from MySQL.  Just includes stuff common to 3.1 & 4.1"
    (
    String name;	 "Name of table"
    String type;	 "Type - MyISAM, InnoDB, etc."
    String rowFormat;	 "Row storage format: Fixed, Dynamic, Compressed"
    int rowCount;        "Number of rows"
    int averageRowLength;"Average row length"
    int dataLength;	 "Length of data file"
    int maxDataLength;   "Maximum length of data file"
    int indexLength;	 "Length of index file"
    int dataFree;	 "Number of allocated but not used bytes of data"
    String autoIncrement; "Next autoincrement value (or NULL)"
    String createTime;	 "Table creation time"
    String updateTime;	 "Table last update time"
    String checkTime;    "Table last checked time (or NULL)"
    )
