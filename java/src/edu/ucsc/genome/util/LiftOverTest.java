package edu.ucsc.genome.util;
import edu.ucsc.genome.qa.lib.*;
import java.io.*;
import java.sql.*;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.Iterator;


class LiftOverTest {

  static void usage() {
    System.out.println(
      "LiftOverTest - reads test cases and runs liftOver command.\n" +

      "usage:\n" +
      "   java LiftOverTest propertiesFile\n" +
      "   java LiftOverTest default\n" +
      "This will use the default properties\n"
      );
    System.exit(-1);
  }

  public static ArrayList getCases(LiftOverTestTarget target) throws Exception {

    ArrayList al = new ArrayList();

    if (!QADBLibrary.checkDriver()) return(al);
    
    HGDBInfo dbinfo = new HGDBInfo(target.server, target.database);
    String dbURL = QADBLibrary.jdbcURL(dbinfo);

    //System.out.println(dbURL);  //debug

    Connection con = DriverManager.getConnection(dbURL);
    Statement stmt = con.createStatement();
    String query = "select * from " + target.table;
    ResultSet rs = stmt.executeQuery(query);
    while (rs.next()) {
        
      String id = rs.getString("id");
      String comment = rs.getString("comment");
      String origAssembly = rs.getString("origAssembly");
      String newAssembly = rs.getString("newAssembly");
      String origChrom = rs.getString("origChrom");
      int origStart = rs.getInt("origStart");
      int origEnd = rs.getInt("origEnd");
      String status = rs.getString("status");
      String message = rs.getString("message");
      String newChrom = rs.getString("newChrom");
      int newStart = rs.getInt("newStart");
      int newEnd = rs.getInt("newEnd");

      LiftOverTestCase newCase = 
        new LiftOverTestCase(id, comment, origAssembly, newAssembly, origChrom,
	                       origStart, origEnd, status, message, newChrom,
			       newStart, newEnd);
      al.add(newCase);
    }
	     
    stmt.close();
    con.close();
    return(al);

  }

  // just one line in file
  // could use Coords object
  public static void writeInputFile(String file, String chrom,
                                    int start, int end) throws Exception {
    FileWriter fout;
    PrintWriter pw;
    String line;

    fout = new FileWriter(file);
    pw = new PrintWriter(fout);
    line = chrom + " " + start + " " + end;
    pw.print(line);
    pw.print("\n");
    pw.close();
    fout.close();
  }

  public static String capitalize(String inputstr) {
    char c1 = inputstr.charAt(0);
    char c2 = Character.toUpperCase(c1);
    Character fullchar = new Character(c2);
    String newstr = fullchar.toString() + inputstr.substring(1);
    return(newstr);
  }
    
  public static boolean checkForFile(String filename) {
    File myfile = new File(filename);
    if (!myfile.exists()) return (false);
    if (!myfile.canRead()) return (false);
    return (true);
  }

  public static boolean runCommand(LiftOverTestCase testcase, boolean verbose) throws Exception {

    String input = "input";
    String output = "success.out";
    String error = "failure.out";
    String chain = "";
    // add to LiftOverTestTarget
    String path = "/cluster/bin/i386";


    try {
      writeInputFile(input, testcase.origChrom, testcase.origStart, testcase.origEnd);
    } catch (Exception e) {
      System.out.println(e.getMessage());
      return (false);
    }

    // use origAssembly and newAssembly to construct path to chain file
    chain = "/gbdb/" + testcase.origAssembly + "/liftOver/";
    String targetAssembly = capitalize(testcase.newAssembly);
    chain = chain + testcase.origAssembly + "To" + targetAssembly + ".over.chain.gz";
    if (!checkForFile(chain)) {
      System.out.println("Cannot find or open " + chain);
      return (false);
    }

    Runtime runtime = Runtime.getRuntime();
    try {
      String command = path + "/liftOver " + input + " " + chain + " ";
      command = command + output + " " + error;
      if (verbose)
        System.out.println(command);
      Process process = runtime.exec(command);
      process.waitFor();
    } catch (Exception e) {
      System.out.println(e.getMessage());
      return (false);
    }

    if (!checkOutput(testcase, output, error)) {
        System.out.println("Mismatch detected");
	return(false);
    }

    return (true);

  }

  public static String getLiftoverMessage(String filename) {
    FileReader fin;
    BufferedReader br;
    String thisline = "";

    try {
      fin = new FileReader(filename);
      br = new BufferedReader(fin);
      // only need to read the first line
      thisline = br.readLine();
      fin.close();
    } catch (Exception e) {
      System.out.println(e.getMessage());
    }

    // System.out.println("message = " + thisline);

    // strip off comment
    thisline = thisline.substring(1);
    return(thisline);
  }

  public static boolean checkOutput(LiftOverTestCase testcase, String output, String error) {

    String expectedResult = testcase.status;
    // System.out.println("Expected result = " + expectedResult);
    if (expectedResult.equals("SUCCESS")) {
      if (!checkForFile(output)) {
        System.out.println(testcase.id + " cannot find or open output file");
	return(false);
      }
      Coords coords = new Coords(output);
      if (!coords.match(testcase.newChrom, testcase.newStart, testcase.newEnd)) {
        System.out.println(testcase.id + " unexpected coords");
	coords.print();
	return (false);
      }
      return(true);
    }

    if (!checkForFile(error)) {
      System.out.println(testcase.id + " cannot find or open error file");
      return (false);
    }
    String message = getLiftoverMessage(error);
    if (!message.equals(testcase.message)) {
      System.out.println(testcase.id + " unexpected error message " + message);
      return (false);
    }
    return (true);
  }

 
  public static void main(String[] args) throws Exception {

    boolean verbose = false;
    int errCount = 0;

    if (args.length != 1)
      usage();
    LiftOverTestTarget target = new LiftOverTestTarget(args[0]);

    ArrayList cases = getCases(target);
    Iterator iter = cases.iterator();

    // make sure CLASSPATH has been set for JDBC driver

    while (iter.hasNext()) {
      LiftOverTestCase testcase = (LiftOverTestCase) iter.next();
      try {
        boolean runStatus = runCommand(testcase, verbose);
	if (!runStatus) {
	  System.out.println("Skipping testcase " + testcase.id);
          System.out.println("----------------------");
	  errCount++;
	  continue;
	}
      } catch (Exception e) {
        System.out.println(e.getMessage());
	continue;
      }
      if (verbose) 
        System.out.println(testcase.id + " command matches testcase");
    }
    System.out.println("Total number of errors = " + errCount);
  }
}

