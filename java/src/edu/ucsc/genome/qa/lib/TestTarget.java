import java.util.Properties;

/**
 *  This container holds properties used by a run of a Test program
 */
public class TestTarget {

  // data
  String machine;
  // could hold database name or the string "all"
  String dbSpec;
  String table;
  boolean quickOn;

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
    dbSpec = properties.getProperty("dbSpec", dbSpec);
    table = properties.getProperty("table", table);
    String quick = properties.getProperty("quick", "false");
    quickOn = quick.equals("true");
  }
    
  public void setDefaults() {
    machine = "hgwbeta.cse.ucsc.edu";
    dbSpec = "hg16";
    table = "knownGene";
    quickOn = false;
 }

}
