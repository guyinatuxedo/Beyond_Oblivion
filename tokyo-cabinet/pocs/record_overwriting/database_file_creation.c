#include <tcutil.h>
#include <tchdb.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

int main(int argc, char **argv){
  TCHDB *hdb;
  int ecode;

  /* create the object */
  hdb = tchdbnew();

  /* open the database */
  if(!tchdbopen(hdb, "fighters.z", HDBOWRITER | HDBOCREAT)){
    ecode = tchdbecode(hdb);
    fprintf(stderr, "open error: %s\n", tchdberrmsg(ecode));
  }

  /* store records */
  if(!tchdbput2(hdb, "gohan", "kamehameha") ||
     !tchdbput2(hdb, "picolo", "finger_thing") ||
     !tchdbput2(hdb, "vegeta", "final_flash")){
    ecode = tchdbecode(hdb);
    fprintf(stderr, "put error: %s\n", tchdberrmsg(ecode));
  }

  /* close the database */
  if(!tchdbclose(hdb)){
    ecode = tchdbecode(hdb);
    fprintf(stderr, "close error: %s\n", tchdberrmsg(ecode));
  }

  /* delete the object */
  tchdbdel(hdb);

  return 0;
}
