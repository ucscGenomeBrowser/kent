package edu.ucsc.genome.qa.monitor;
import edu.ucsc.genome.qa.lib.*;
import java.io.*;
import java.sql.*;

import java.util.Iterator;
import java.util.Properties;

/** 
 *  Connect to where mod_log_sql is dumping Apache access logs.
 *  Check for new rows within the last number of minutes.
 */

public class ApacheMonitor {

  static void usage() {
    System.out.println(
      "\nApacheMonitor - read logs from mod_log_sql\n" +
      "Check for new rows within the last number of minutes.\n" +
      
      "usage:\n" +
      "   java ApacheMonitor propertiesFile [verbose]\n" +
      "where properties files may contain sourceMachine, sourceDB, sourceTable, " +
      "targetMachine, errorCodes, minutes.\n" +
      "   java ApacheMonitor default\n" +
      "This will use the default properties\n" +
      "Verbose will report even if no errors found\n"
      );
    System.exit(-1);
  }

  public static void main(String[] args) {

    boolean debug = false;
    String mode  = "";

    /* Process command line properties, and load them into machine and table. */
    if (args.length < 1 || args.length > 2)
        usage();
    if (args.length == 2) {
        mode = args[1];
    }
    LogTarget target = new LogTarget(args[0]);

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
    if (!dbinfo.validate()) return;
    String myurl = QADBLibrary.jdbcURL(dbinfo);

    try {
      Connection conn = DriverManager.getConnection(myurl);
      Statement stmt = conn.createStatement();

      // all machines?
      boolean allMachines = false;
      String machine = target.targetMachine;
      if (machine.equals("all")) allMachines = true;

      // get mysql time, set time delta
      String timequery = "SELECT unix_timestamp(now()) AS secondsNow";
      ResultSet timeRS = stmt.executeQuery(timequery); 
      timeRS.next(); 
      int secondsNow = timeRS.getInt("secondsNow");
      int secondsDelta = target.minutes * 60;
      int timeDelta = secondsNow - secondsDelta;

      // check for any rows within time delta
      String nullquery = "SELECT COUNT(*) AS cnt FROM ";
      nullquery = nullquery + target.sourceTable + " ";
      nullquery = nullquery + "WHERE time_stamp > " + timeDelta;
      if (!allMachines) {
        nullquery = nullquery + " AND machine_id = " + target.targetMachine;
      }
      //# System.out.println(nullquery);
      ResultSet nullRS = stmt.executeQuery(nullquery);
      nullRS.next();
      int nullcnt = nullRS.getInt("cnt");
      //# System.out.println("Count of rows with any status code = " + nullcnt);


      // check for matching rows
      String testquery = "SELECT COUNT(*) AS cnt FROM ";
      testquery = testquery + target.sourceTable + " ";
      testquery = testquery + "WHERE status = " + target.errorCode + " ";
      testquery = testquery + "AND time_stamp > " + timeDelta;

      if (!allMachines) {
        testquery = testquery + " and machine_id = " + target.targetMachine;
      }

      //# System.out.println(testquery);
      ResultSet testRS = stmt.executeQuery(testquery);
      testRS.next();
      int cnt = testRS.getInt("cnt");
      //# System.out.println("Count of matching rows = " + cnt);

      // set to print only if errors detected or if verbose mode
      if (cnt != 0 || mode.equals("verbose")) {
        System.out.println(nullquery);
        System.out.println("Count of rows with any status code = " + nullcnt);
        System.out.println(testquery);
        System.out.println("Count of matching rows = " + cnt);
      }


      // if nothing found, we're done
      if (cnt == 0) System.exit(0);

      // get all matching rows
      String listquery = "SELECT request_uri, machine_id, virtual_host, time_stamp FROM ";
      listquery = listquery + target.sourceTable + " ";
      listquery = listquery + "WHERE status = " + target.errorCode + " ";
      listquery = listquery + "AND time_stamp > " + timeDelta;
      if (!allMachines) {
        listquery = listquery + " AND machine_id = " + target.targetMachine;
      }

      System.out.println(listquery);

      ResultSet listRS = stmt.executeQuery(listquery);
      while (listRS.next()) {
        String request_uri = listRS.getString("request_uri");
        String machine_id  = listRS.getString("machine_id");
        String virtual_host = listRS.getString("virtual_host");
	int time_stamp = listRS.getInt("time_stamp");
	int deltaSeconds = secondsNow - time_stamp;
	int deltaMinutes = deltaSeconds / 60;
	System.out.print("Status " + target.errorCode + " from " + request_uri + " on ");
	System.out.println(machine_id + "; " + deltaMinutes + " minutes ago");
        // if (virtual_host != "genome.cse.ucsc.edu" {
	//   System.out.println("    from " + virtual_host);
        // }
      }

      stmt.close();
      conn.close();

    } catch (Exception e) {
      System.err.println(e.getMessage());
    }

  }
}
