import java.io.*;
import java.net.*;
import java.sql.*;
import java.util.Properties;
import lib.*;

/**
 *  Checks for validity of database connection and metadatabase info.
 */
public class DBInfoUtil {

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

    DBInfo metadbinfo = 
       new DBInfo("hgwbeta", "hgcentraltest", userRead, passwordRead);

    if (!metadbinfo.validate()) return;

  /*  incomplete ?  no DBInfo objects with this signature
    DBInfo dbinfo = new DBInfo("localhost", assembly, userRead, passwordRead);
    if (!dbinfo.validate()) {
      System.out.println("Cannot connect to database for " + assembly);
      continue;
    }
   */

  }
}
