package edu.ucsc.genome.qa.lib;
import java.util.Properties;

/**
 *  This container holds properties used by a run of a Test program
 */
public class TestTarget {

  // data
  public String machine;
  // could hold database name or the string "all"
  public String server;
  public String dbSpec;
  public String table;
  public boolean quickOn;
  public int zoomCount;

  // constructors

 /**
  * Constructor for database query parameters.
  * 
  * @param propFileName    File with properties
  */
  public TestTarget(String propFileName) {
    setDefaults();
    if (propFileName.equals("default")) {
      return;
    }

    Properties properties = new Properties();
    properties = QALibrary.readProps(propFileName);
    machine = properties.getProperty("machine", machine);
    server = properties.getProperty("server", machine);
    dbSpec = properties.getProperty("dbSpec", dbSpec);
    table = properties.getProperty("table", table);
    String quick = properties.getProperty("quick", "false");
    quickOn = quick.equals("true");
    String zoomString = String.valueOf(zoomCount);
    zoomString = properties.getProperty("zoomCount", zoomString);
    zoomCount = Integer.parseInt(zoomString);
  }
    
  public void setDefaults() {
    machine = "localhost";
    dbSpec = "hg17";
    table = "refGene";
    quickOn = false;
    zoomCount = 1;
 }

  public String toString() {
    return
    "machine " + machine + "\n" + 
    "server " + server + "\n" +
    "quick " + quickOn + "\n" +
    "dbSpec " + dbSpec + "\n" +
    "table " + table + "\n" +
    "zoomCount " + zoomCount + "\n";
  }

}
