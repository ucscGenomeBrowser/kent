import java.sql.*;

/**
 *  This container holds all database properties 
 */
public class DBInfo {

  // data
  String machine;
  String metadatabase;
  String user;
  String password;

  // constructors

 /**
  * Constructor for database query parameters.
  * 
  * @param machineVar      The machine
  * @param metadatabaseVar The name of the metadatabase
  * @param userVar         The username
  * @param passwordVar     The password
  */
  public DBInfo(String machineVar, String metadatabaseVar, 
         String userVar, String passwordVar) {

    machine = machineVar;
    metadatabase = metadatabaseVar;
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
    dbURL = dbURL + "/" + metadatabase;
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
