import java.io.*;
import java.net.*;
import java.sql.*;
import java.util.Properties;

/**
 *  Checks for validity of database connection and metadatabase info.
 */
public class HGDBInfoTest {

 /**
  *  Runs program to check for database validity.
  */
  public static void main(String[] args) {

    // make sure CLASSPATH has been set for JDBC driver
    if (!QADBLibrary.checkDriver()) return;
    
    // get read access to database
    Properties dbloginread;
    try {
      dbloginread = QALibrary.readProps("javadbconf.read");
    } catch (Exception e) {
      System.out.println("Cannot read javadbconf.read");
      return;
    }
    String userRead = dbloginread.getProperty("login");
    String passwordRead = dbloginread.getProperty("password");

    HGDBInfo metadbinfo = 
       new HGDBInfo("hgwbeta", "hgcentraltest", userRead, passwordRead);

    if (!metadbinfo.validate()) return;

  /*  incomplete ?  no HGDBInfo objects with this signature
    HGDBInfo dbinfo = new HGDBInfo("localhost", assembly, userRead, passwordRead);
    if (!dbinfo.validate()) {
      System.out.println("Cannot connect to database for " + assembly);
      continue;
    }
   */

  }
}
