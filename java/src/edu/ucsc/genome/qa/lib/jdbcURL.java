
 /** 
  *  Get string with machine/database/user/password. 
  * 
  *  @return   Database URL with machine, database, user, password
  */
  public static String jdbcURL(DBInfo dbinfo) {
    String dbURL = "jdbc:mysql://" + dbinfo.machine;
    dbURL = dbURL + "/" + dbinfo.metadatabase;
    dbURL = dbURL + "?user=" + dbinfo.user;
    dbURL = dbURL + "&password=" + dbinfo.password;
    return dbURL;
  }
