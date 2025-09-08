// SPDX-License-Identifier: MIT; (c) 2025 Andrew D Smith (author)
/* bigCompositeUpdate - CGI to update bigComposite track visibilities */
#include "common.h"
#include "hash.h"
#include "jksql.h"
#include "htmshell.h"
#include "web.h"
#include "cheapcgi.h"
#include "cart.h"
#include "hui.h"

/* Global Variables */
struct cart *cart;             /* CGI and other variables */
struct hash *oldVars = NULL;

void doMiddle(struct cart *theCart) {
  const int buffer_size = 128;
  cart = theCart;

  char *mdid = cgiString("mdid");  // metadata id (e.g., mb2 for MethBase2)
  char mdid_dt[buffer_size];
  snprintf(mdid_dt, buffer_size, "%s_%s", mdid, "dt");
  char mdid_de[buffer_size];
  snprintf(mdid_de, buffer_size, "%s_%s", mdid, "de");
  char wc_mdid[buffer_size];
  snprintf(wc_mdid, buffer_size, "*_%s", "mb2");

  // clear out previous variables to avoid having to use =0 for
  // non-visible data-type / data set combinations
  cartRemoveLike(cart, wc_mdid);
  cartRemovePrefix(cart, mdid);

  struct dyString *cgi_encoded = cgiUrlString();
  struct slName *dt_list = NULL;
  struct slName *de_list = NULL;
  char *raw_cgi = dyStringContents(cgi_encoded);
  char *var = NULL, *val = NULL;
  struct dyString *updated_cgi = newDyString(1024);
  while (cgiParseNext(&raw_cgi, &var, &val)) {
    if (sameWord(var, mdid_dt))
      slAddHead(&dt_list, newSlName(val));
    else if (sameWord(var, mdid_de))
      slAddHead(&de_list, newSlName(val));
    else cgiEncodeIntoDy(var, val, updated_cgi);
  }

  char track_name[buffer_size];
  struct slName *de_itr = NULL;
  for (de_itr = de_list; de_itr; de_itr = de_itr->next) {
    struct slName *dt_itr = NULL;
    for (dt_itr = dt_list; dt_itr; dt_itr = dt_itr->next) {
      snprintf(track_name, buffer_size,
	       "%s_%s_%s_sel", mdid, de_itr->name, dt_itr->name);
      cgiEncodeIntoDy(track_name, "1", updated_cgi);
    }
  }
  cartParseOverHash(cart, dyStringContents(updated_cgi));
  cartRemove(cart, "mdid");  // could clash someday
  cartCheckout(&cart);

  dyStringFree(&cgi_encoded);
  dyStringFree(&updated_cgi);

  // send response: success, and no content
  printf("Status: 204 No Content\r\n");
  printf("\r\n");
  fflush(stdout);
}

/* Null terminated list of CGI Variables we don't want to save
 * permanently. */
char *excludeVars[] = {"Submit", "submit", NULL,};

int main(int argc, char *argv[])
/* Process command line. */
{
  cgiSpoof(&argc, argv);
  cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldVars);
  return 0;
}
