import java.io.*;
import java.net.*;

import com.meterware.httpunit.*;
import org.xml.sax.*;

class Sep16 {
  public static void main(String[] args) {

    String url1, url2, url3;

    String machine = "hgw7.cse.ucsc.edu";
    String assembly = "hg16";
    String defaultPos = "chr4:56214201-56291736";
    String track = "cytoBand";

    WebConversation wc = new WebConversation();

    url1 = "http://" + machine;
    url1 = url1 + "/cgi-bin/hgTracks?db=" + assembly;
    url2 = url1 + "&position=" + defaultPos;

    // hide all and turn off ruler
    url3 = url2 + "&hgt.hideAll=yes&ruler=off";

    WebRequest req = new GetMethodWebRequest(url3);

    try {
      WebResponse page = wc.getResponse(req);
      WebForm form = page.getForms()[0];
      form.setParameter(track, "full");
      // HGWebLibrary.refreshHGTracks(page);
      SubmitButton refreshButton = 
        form.getSubmitButton("submit", "refresh");
      refreshButton.click();
      page = wc.getCurrentPage();
      // HGWebLibrary.printHGTracksImage(page);
      // String elements[] = page.getElementNames();
      // for (int e=0; e < elements.length; e++) {
        // System.out.println(elements[e]);
      // }
      // HTMLElement map = page.getElementWithID("map");

      // WebImage image = page.getImages()[0];
      // String params[] = image.getParameterNames();
      // for (int i=0; i < params.length; i++) {
        // System.out.println(params[i]);
        // String values[] = image.getParameterValues(params[i]);
        // for (int j=0; j < values.length; j++) {
          // System.out.println("  " + values[j]);
        // }
      // }     
      WebLink links[] = page.getLinks();
      for (int i=0; i < links.length; i++) {
        String url4 = links[i].getURLString();
        System.out.println(links[i].asText() + " " + url4);
        // String params[] = links[i].getParameterNames();
        // for (int j=0; j < params.length; j++) {
          // System.out.println("  " + params[j]);
        // }
      }
    } catch (Exception e) {
      System.out.println(e.getMessage());
    }
      
  }
}
