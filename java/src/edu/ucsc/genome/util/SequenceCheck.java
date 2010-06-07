package edu.ucsc.genome.util;
import edu.ucsc.genome.qa.lib.*;
import java.io.*;
import java.sql.*;
import java.util.ArrayList;
import java.util.Iterator;

class SequenceCheck {

  static void usage() {
    System.out.println(
      "SequenceCheck - calls nibFrag, compares sequence value.\n" +

      "usage:\n" +
      "   java SequenceCheck\n" 
      );
    System.exit(-1);
  }

  public static ArrayList getTargets(String server, String database) throws Exception {

    ArrayList al = new ArrayList();

    if (!QADBLibrary.checkDriver()) return(al);
    
    HGDBInfo dbinfo = new HGDBInfo(server, database);
    String dbURL = QADBLibrary.jdbcURL(dbinfo);

    Connection con = DriverManager.getConnection(dbURL);
    Statement stmt = con.createStatement();
    String query = "select chrom, chromStart, chromEnd, reference from encodeIndelsUniq";
    ResultSet rs = stmt.executeQuery(query);
    while (rs.next()) {
        
      String chrom = rs.getString("chrom");
      int start = rs.getInt("chromStart");
      int end = rs.getInt("chromEnd");
      String sequence = rs.getString("reference");
      // skip simple insertions
      if (sequence.equals("-")) continue;

      SequenceCheckTarget newTarget = new SequenceCheckTarget(chrom, start, end, sequence);
      al.add(newTarget);
    }
	     
    stmt.close();
    con.close();
    System.out.println(al.size() + " targets found");
    return(al);

  }

  public static boolean checkForFile(String filename) {
    File myfile = new File(filename);
    if (!myfile.exists()) return (false);
    if (!myfile.canRead()) return (false);
    return (true);
  }

  public static boolean runCommand(SequenceCheckTarget target) throws Exception {

    String chrom = target.chrom;
    String chromNum = chrom.substring(3);
    String path = "/cluster/bin/i386";

    String nib = "/gbdb/hg16/nib/" + chrom + ".nib";
    if (!checkForFile(nib)) {
      System.out.println("Cannot find or open " + nib);
      return (false);
    }

    Runtime runtime = Runtime.getRuntime();
    try {
      String command = path + "/nibFrag " + nib + " " + target.start + " " + target.end + " + out.fa";
      System.out.println(command);
      Process process = runtime.exec(command);
      process.waitFor();
    } catch (Exception e) {
      System.out.println(e.getMessage());
      return (false);
    }

    if (!checkOutput("out.fa", target.sequence)) {
        System.out.println("Mismatch detected at " + target.chrom + ":" + target.start + "-" + target.end);
	return(false);
    }

    return (true);

  }

  public static String readOutput(String filename) {
    FileReader fin;
    BufferedReader br;
    String thisline = "";
    String skipline = "";

    try {
      fin = new FileReader(filename);
      br = new BufferedReader(fin);
      // skip first line
      skipline = br.readLine();
      thisline = br.readLine();
      fin.close();
    } catch (Exception e) {
      System.out.println(e.getMessage());
    }

    return(thisline);
  }

  public static boolean checkOutput(String filename, String sequence) {

    String expectedResult = readOutput(filename);
    expectedResult = expectedResult.toLowerCase();
    System.out.println("Expected result = " + expectedResult);
    sequence = sequence.toLowerCase();
    if (expectedResult.equals(sequence)) {
	return(true);
    }
    return (false);
  }

 
  public static void main(String[] args) throws Exception {

    ArrayList targets = getTargets("localhost", "hg16");
    Iterator iter = targets.iterator();

    while (iter.hasNext()) {
      SequenceCheckTarget target = (SequenceCheckTarget) iter.next();
      try {
        runCommand(target);
	Thread.sleep(1000);
      } catch (Exception e) {
        System.out.println(e.getMessage());
	continue;
      }
    }
  }
}

