package edu.ucsc.genome.qa.lib;
import java.util.Properties;
import com.meterware.httpunit.*;

/**
 *  Get one URL.  Record how long it takes and the
 *  return status.
 */
public class CheckUrl {

  // data
  public String url;	     // The URL we are checking.
  public double runTime;     // Seconds it takes to get complete response.
  public int htmlStatus;     // 200 if all is ok
  public String errorReport; // Web's page error message (even if 200 status), null if no error. 

  public CheckUrl(String url) throws Exception {
      this.url = url;
      long startTime = System.currentTimeMillis();

      WebConversation wc = new WebConversation();
      WebRequest req = new GetMethodWebRequest(url);
      WebResponse page = wc.getResponse(req);
      htmlStatus  = page.getResponseCode();
      if (htmlStatus == 200) {// Might be a error still that program catches
	  String text = page.getText();
	  String errStartMsg = "<!-- HGERROR-START -->";
	  String errEndMsg = "<!-- HGERROR-END -->";
	  int errStartIx = text.indexOf(errStartMsg);
	  if (errStartIx >= 0) {
	      errStartIx += errStartMsg.length();
	      int errEndIx = text.indexOf(errEndMsg);
	      if (errEndIx < errStartIx)
	          errEndIx = errStartIx + 100;
	      errorReport = text.substring(errStartIx, errEndIx);
	  }
      }
      long endTime = System.currentTimeMillis();
      runTime = (endTime - startTime) * 0.001;
  }


  /**
   * Print out basic info on this URL check. 
   */
  public void report()  {
     System.out.println("runTime " + runTime  + ", " +
                        "htmlStatus " + htmlStatus + ", " +
			"errorReport " + errorReport );
  }

  public static void main(String args[]) throws Exception {
     CheckUrl checkUrl = new CheckUrl("http://genome.ucsc.edu/cgi-bin/hgNear?hgsid=29699799&org=Human&db=hg16&near_search=skljfs%3Bldjjj&submit=Go%21&near.order=knownExpGnfU95&near.count=50");
     checkUrl.report();
  }
}
