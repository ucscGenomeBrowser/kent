package edu.ucsc.genome.qa.monitor;
import edu.ucsc.genome.qa.lib.*;
import java.io.*;
import java.sql.*;

import java.util.Calendar;
import java.util.Properties;
import java.util.regex.*;

/** 
 *  Wrapper around "show table status".
 */

public class DatabaseMonitor {




  static void usage() {
    System.out.println(
      "DatabaseMonitor - \n" +
      "   get database status.\n" +
      
      "usage:\n" +
      "   java DatabaseMonitor propertiesFile\n" +
      "where properties file may contain sourceMachine and sourceDB.\n " +
      "   java DatabaseMonitor default\n" +
      "This will use the default properties\n"
      );
    System.exit(-1);
  }

  public static void main(String[] args) {

    boolean debug = false;

    // first figure out what day it is
    Calendar calNow = Calendar.getInstance();
    int dayNow = calNow.get(Calendar.DATE);
    int monthNow = calNow.get(Calendar.MONTH);
    int yearNow = calNow.get(Calendar.YEAR);

    /* Process command line properties, and load them into machine and table. */
    if (args.length != 1)
        usage();
    DBTarget target = new DBTarget(args[0]);
    System.out.println(target.sourceDB);

    // make sure CLASSPATH has been set for JDBC driver
    if (!QADBLibrary.checkDriver()) return;
    
    // set DB connection 
    HGDBInfo dbinfo; 
    try {
      dbinfo = new HGDBInfo(target.sourceMachine, target.sourceDB);
    } catch (Exception e) {
      System.out.println(e.toString());
      return;
    }
    if (!dbinfo.validate()) {
      System.out.println("dbinfo invalid");
      return;
    }
    String myurl = QADBLibrary.jdbcURL(dbinfo);

    try {
      Connection conn = DriverManager.getConnection(myurl);
      Statement stmt = conn.createStatement();
    
      String query = "show table status";

      ResultSet myrs = stmt.executeQuery(query); 

      while (myrs.next()) {
        // String name = myrs.getString(1);
        String name = myrs.getString("Name");
	// skip certain names
	if (name.equals("tableDescriptions")) continue;
	Pattern trackdbPattern = Pattern.compile("trackDb*");
	Matcher trackdbMatcher = trackdbPattern.matcher(name);
	if (trackdbMatcher.matches()) continue;
	Pattern findspecPattern = Pattern.compile("hgFindSpec*");
	Matcher findspecMatcher = findspecPattern.matcher(name);
	if (findspecMatcher.matches()) continue;
        // int rows = myrs.getInt(4);
        // int rows = myrs.getInt("Rows");
        // Date createDate = myrs.getDate(11);
        // Date createDate = myrs.getDate("Create_time");
        // Date updateDate = myrs.getDate(12);
        Date updateDate = myrs.getDate("Update_time");
        // Date checkDate = myrs.getDate(13);
        // Date checkDate = myrs.getDate("Check_time");
	// long updateTime = updateDate.getTime();
	// convert update date into Calendar
	Calendar calUpdate = Calendar.getInstance();
	calUpdate.setTime(updateDate);
        int dayUpdate = calUpdate.get(Calendar.DATE);
        int monthUpdate = calUpdate.get(Calendar.MONTH);
        int yearUpdate = calUpdate.get(Calendar.YEAR);
	if (dayUpdate == dayNow && monthUpdate == monthNow && yearUpdate == yearNow) {
	  System.out.println(name);
        }
      }
      stmt.close();
      conn.close();

    } catch (Exception e) {
      System.err.println(e.getMessage());
    }

  }
}
