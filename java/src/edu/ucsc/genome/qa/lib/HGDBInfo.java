import java.sql.*;

/**
 *  This container holds all database properties 
 */
public class HGDBInfo {

  // data
  String machine;
  String database;
  String user;
  String password;

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
    
    } catch (Exception e) {
      System.err.println(e.getMessage());
      return false;
    }
    return true;
  }
}
