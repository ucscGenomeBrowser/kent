// output maximum coord for each scaffold
// assuming at least one row in table
package edu.ucsc.genome.util;
import edu.ucsc.genome.qa.lib.*;
import java.io.*;
import java.sql.*;

class GetMaxCoord {
  public static void main(String[] args) {

    String scaffoldName = "";
    boolean firstRow = true;
    int maxCoord = 0;

    // make sure CLASSPATH has been set for JDBC driver
    if (!QADBLibrary.checkDriver()) return;
                                                                                  
    HGDBInfo dbinfo;
    try {
       dbinfo = new HGDBInfo("localhost", "bosTau1");
    } catch (Exception e) {
       System.out.println(e.toString());
       return;
    }

    if (!dbinfo.validate()) return;

    String url = QADBLibrary.jdbcURL(dbinfo);

    try {
      Connection con = DriverManager.getConnection(url);
      Statement stmt = con.createStatement();
      
      String query = "select name, start, end from scaffoldCoords";
      
      ResultSet rs = stmt.executeQuery(query);

      while (rs.next()) {
        String name = rs.getString("name");
	int start = rs.getInt("start");
	int end = rs.getInt("end");
	if (firstRow) {
	  firstRow = false;
	  scaffoldName = name;
	  if (start != 1) {
	    System.out.println(scaffoldName + " starts at " + start);
	  }
	} else if (!name.equals(scaffoldName)) {
	  System.out.println(scaffoldName + " " + maxCoord);
	  scaffoldName = name;
	}
	maxCoord = end;

      } 
      System.out.println(scaffoldName + "\t" + maxCoord);

      stmt.close();
      con.close();

    } catch (Exception e) {
      System.err.println(e.getMessage());
      return;
    }

  }
}

