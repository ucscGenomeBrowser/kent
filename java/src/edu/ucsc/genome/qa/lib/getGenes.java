 /** 
  *  Get genes from Known Genes (or equivalent) table
  * 
  *  @param  dbinfo   The machine, metadatabase, user and password.
  *  @param  table    Table with gene names.
  *  @param  pb       True if proteome browser query (needs proteinID).
  *
  *  @return       List of all Known Genes.  Includes proteinID if pb is true.
  */
  public static ArrayList getGenes(DBInfo dbinfo, String table, boolean pb) {

    ArrayList kgList = new ArrayList();
    String    dbURL  = jdbcURL(dbinfo);
    String    query  = "SELECT name, chrom, txStart, txEnd, proteinID FROM " + table;

    try {

      Connection con  = DriverManager.getConnection(dbURL);
      Statement  stmt = con.createStatement();
      ResultSet  rs   = stmt.executeQuery(query);
      while (rs.next()) {
        String nameVar      = rs.getString("name");
        String chromVar     = rs.getString("chrom");
        int txStartVar      = rs.getInt("txStart");
        int txEndVar        = rs.getInt("txEnd");
        String proteinIdVar = rs.getString("proteinID");
        KnownGene kg;
        if (pb) {
          kg = new KnownGene (nameVar, chromVar, txStartVar, txEndVar, proteinIdVar);
        } else {
          kg = new KnownGene (nameVar, chromVar, txStartVar, txEndVar);
        }
        kgList.add(kg);
      }
      stmt.close();
      con.close();
    
    } catch (Exception e) {
      System.err.println(e.getMessage());
    }

    return (kgList);
  }
