package edu.ucsc.genome.qa.lib;
import java.sql.*;

/**
 *  This container holds all database properties 
 */
public class HGDBInfo {

  // data
  public String machine;
  public String database;
  public String user;
  public String password;

  // constructors

 /**
  * Constructor for database query parameters.
  * 
  * @param machineVar      The machine
  * @param databaseVar 	   The name of the database
  * @param userVar         The username
  * @param passwordVar     The password
  */
  public HGDBInfo(String machineVar, String databaseVar, 
         String userVar, String passwordVar) {

    machine = machineVar;
    database = databaseVar;
    user = userVar;
    password = passwordVar;
  }

 /**
  * Constructor for database query parameters.  This
  * takes the user and password from hg.conf.
  * 
  * @param machineVar      The machine
  * @param databaseVar 	   The name of the database
  */
  public HGDBInfo(String machineVar, String databaseVar) throws Exception {

    HgConf hgConf = new HgConf();
    machine = machineVar;
    database = databaseVar;
    user = hgConf.getProperty("db.user");
    password = hgConf.getProperty("db.password");
  }


 /**
  * Checks for valid conenction to database
  * 
  * @return   True if database connection is successful
  */ 
  public boolean validate() {

    String dbURL = "jdbc:mysql://" + machine;
    dbURL = dbURL + "/" + database;
    dbURL = dbURL + "?user=" + user;
    dbURL = dbURL + "&password=" + password;

    try {
      Connection con = DriverManager.getConnection(dbURL);
      con.close();
    } catch (Exception e) {
      System.err.println(e.getMessage());
      return false;
    }
    return true;
  }
}
