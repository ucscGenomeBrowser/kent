package edu.ucsc.genome.qa.filter;
import edu.ucsc.genome.qa.lib.*;
import java.io.*;
import java.sql.*;
import java.util.HashSet;
import java.util.Iterator;

class ScoreFilter {
  public static void main(String[] args) {

    Connection read_con, write_con;
    Statement read_stmt, write_stmt;
    String database = "qa";
    String inputTableName = "blast2";
    String outputTableName = "blast3";
    int matchCount = 5;

    if (!QADBLibrary.checkDriver()) {
      System.err.println("Can't get JDBC driver");
      return;
    }

    // get list of unique qNames
    // table is indexed by qName
    HashSet qNames = QALibrary.getSetFromFile("data/idlist");

    // try {
      // Class.forName("com.mysql.jdbc.Driver");
    // } catch (ClassNotFoundException e) {
      // System.err.println("Can't get mysql jdbc driver");
      // return;
    // }


    try {
      HGDBInfo dbread = new HGDBInfo("localhost", database);
      String readURL = QADBLibrary.jdbcURL(dbread);
      HGDBInfo dbwrite = new HGDBInfo("localhost", database);
      String writeURL = QADBLibrary.jdbcURL(dbwrite);

      read_con = DriverManager.getConnection(readURL);
      write_con = DriverManager.getConnection(writeURL);
      read_stmt = read_con.createStatement();
      write_stmt = write_con.createStatement();
      Iterator qNameIter = qNames.iterator();
      while (qNameIter.hasNext()) {
        // get all rows that match this qName
        String qName = (String) qNameIter.next();
	System.out.println(qName);
        String query = 
	  "select tName, score, strand, qStart, qEnd, tStart, tEnd, tSize";
	query = query + " from " + inputTableName;
	query = query + " where qName = ";
	query = query + "\"" + qName + "\"";
	// System.out.println(query);
        ResultSet rs = read_stmt.executeQuery(query);

	// iterate through matches
	// output maxCount rows
	// each time through, build a set of found tNames
	// don't use the same tName twice
	int pos = 0;
        HashSet allNames = new HashSet();
        while (rs.next()) {
          String tName = rs.getString("tName");
	  if (pos > 0) {
	    if (allNames.contains(tName)) continue;
	  }
	  allNames.add(tName);
	  double score = rs.getDouble("score");
	  String strand = rs.getString("strand");
	  int qStart = rs.getInt("qStart");
	  int qEnd = rs.getInt("qEnd");
	  int tStart = rs.getInt("tStart");
	  int tEnd = rs.getInt("tEnd");
	  int tSize = rs.getInt("tSize");

	  String insert = "insert into " + outputTableName;
	  insert = 
	    insert + " (strand, qName, qStart, qEnd, tName, tStart, tEnd, tSize, score) ";
	  insert = insert + "values(\"" + strand + "\", ";
	  insert = insert + "\"" + qName + "\", ";
	  insert = insert + qStart + ", " + qEnd + ", ";
	  insert = insert + "\"" + tName + "\", ";
	  insert = insert + tStart + ", " + tEnd + ", ";
	  insert = insert + tSize + ", ";
	  insert = insert + score + ")";
	  // System.out.println(insert);
	  write_stmt.execute(insert);
	  pos++;
	  if (pos == matchCount) break;
	}
      } 

      read_stmt.close();
      read_con.close();
      write_stmt.close();
      write_con.close();

    } catch (Exception e) {
      System.err.println(e.getMessage());
      System.err.println("Can't get connection to database");
      return;
    }

  }
}

