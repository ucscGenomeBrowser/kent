package edu.ucsc.genome.util;
import edu.ucsc.genome.qa.lib.*;
import java.io.*;
import java.sql.*;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.Iterator;


class LiftOverRescue {

  public static ArrayList getCases() throws Exception {

    ArrayList al = new ArrayList();

    if (!QADBLibrary.checkDriver()) return(al);
    
    HGDBInfo dbinfo = new HGDBInfo("localhost", "hg17");
    String dbURL = QADBLibrary.jdbcURL(dbinfo);

    Connection con = DriverManager.getConnection(dbURL);
    Statement stmt = con.createStatement();
    String query = "select * from encodeIndels where chrom = 'chr7' ";
    query = query + " and chromStart > 125633145 and chromEnd < 126796333";
    ResultSet rs = stmt.executeQuery(query);
    while (rs.next()) {
        int bin = rs.getInt("bin");
        String chrom = rs.getString("chrom");
        int chromStart = rs.getInt("chromStart");;
        int chromEnd = rs.getInt("chromEnd");
        String name = rs.getString("name");
        int score = rs.getInt("score");
        String strand= rs.getString("strand");
        int thickStart = rs.getInt("thickStart");
        int thickEnd = rs.getInt("thickEnd");
        int reserved = rs.getInt("reserved");
        String traceName = rs.getString("traceName");
        String traceId = rs.getString("traceId");
        int tracePos = rs.getInt("tracePos");
        String traceStrand = rs.getString("traceStrand");
        String variant = rs.getString("variant");
        String reference = rs.getString("reference");

      EncodeIndels newRow = 
        new EncodeIndels(bin, chrom, chromStart, chromEnd, name, score,
	                 strand, thickStart, thickEnd, reserved, traceName,
			 traceId, tracePos, traceStrand, variant, reference);
      al.add(newRow);
    }
	     
    stmt.close();
    con.close();
    System.out.println("rows found = " + al.size());
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

    
  public static boolean checkForFile(String filename) {
    File myfile = new File(filename);
    if (!myfile.exists()) return (false);
    if (!myfile.canRead()) return (false);
    return (true);
  }

  public static boolean runCommand(EncodeIndels indel, PrintWriter pw) throws Exception {

    String input = "/tmp/input";
    String output = "/tmp/success.out";
    String error = "/tmp/failure.out";
    String chain = "/gbdb/hg17/liftOver/hg17ToHg16.over.chain";
    String path = "/cluster/bin/i386";

    if (!checkForFile(chain)) {
      System.out.println("Cannot find or open " + chain);
      return (false);
    }

    try {
      writeInputFile(input, indel.chrom, indel.chromStart, indel.chromEnd);
    } catch (Exception e) {
      System.out.println(e.getMessage());
      return (false);
    }

    Runtime runtime = Runtime.getRuntime();
    try {
      String command = path + "/liftOver " + input + " " + chain + " ";
      command = command + output + " " + error;
      Process process = runtime.exec(command);
      process.waitFor();
    } catch (Exception e) {
      System.out.println(e.getMessage());
      return (false);
    }

    if (!checkForFile(output)) {
      System.out.println(" cannot find or open output file");
      return(false);
    }

    String newCoords = readOutput(output);
    if (newCoords == null) {
      System.out.println("no lift for:");
      // change this to indel.print();
      System.out.println("chrom = " + indel.chrom);
      System.out.println("start = " + indel.chromStart);
      System.out.println("end = " + indel.chromEnd);
      System.out.println("name = " + indel.name);
      System.out.println("traceName = " + indel.traceName);
      return(true);
    }

    String line = newCoords;
    line = line + " " + indel.name;
    line = line + " " + indel.score;
    line = line + " " + indel.strand;
    line = line + " " + indel.thickStart;
    line = line + " " + indel.thickEnd;
    line = line + " " + indel.reserved;
    line = line + " " + indel.traceName;
    line = line + " " + indel.traceId;
    line = line + " " + indel.tracePos;
    line = line + " " + indel.traceStrand;
    line = line + " " + indel.variant;
    line = line + " " + indel.reference;
    pw.print(line);
    System.out.println(line);
    pw.print("\n");

    // if (checkForFile(error)) {
    // }

    return (true);

  }

  public static String readOutput(String filename) {
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

    return(thisline);
  }


 
  public static void main(String[] args) throws Exception {

    boolean verbose = false;
    int errCount = 0;

    if (args.length != 1) {
      System.out.println("Please give the name of the output file");
      System.exit(-1);
    }

    FileWriter fout = new FileWriter(args[0]);
    PrintWriter pw = new PrintWriter(fout);

    ArrayList cases = getCases();
    Iterator iter = cases.iterator();

    while (iter.hasNext()) {
      EncodeIndels indel = (EncodeIndels) iter.next();
      try {
        boolean runStatus = runCommand(indel, pw);
	if (!runStatus) {
	  System.out.println("Skipping case " + indel.traceName);
          System.out.println("----------------------");
	  errCount++;
	  continue;
	}
      } catch (Exception e) {
        System.out.println(e.getMessage());
	continue;
      }
    }

    System.out.println("Total number of errors = " + errCount);
    pw.close();
  }
}

