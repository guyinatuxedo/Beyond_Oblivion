# Usage

So this document details simply how to use the Tokyocabinet database. This database is an API that is called from other code, so we will be covering the various API calls that can be made. This will all be done from C.

In here, there are 5 different APIs. Here is a listing:

```
*	Utility API - 
*	Hash Database API - 
*	B+ Tree Database API - 
*	Fixed-length Database API - 
*	Table Database API - 
*	Abstract Database API -
```


## Compiling

So since this database is effectively and API, this will cover how to compile C code which has it.

Let's say we want to compile this code, which is effectively a copy of one of the examples:

```
$	cat test.c
#include <tcutil.h>
#include <tcfdb.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

int main(int argc, char **argv){
  TCFDB *fdb;
  int ecode;
  char *key, *value;

  /* create the object */
  fdb = tcfdbnew();

  /* open the database */
  if(!tcfdbopen(fdb, "casket.tcf", FDBOWRITER | FDBOCREAT)){
    ecode = tcfdbecode(fdb);
    fprintf(stderr, "open error: %s\n", tcfdberrmsg(ecode));
  }


  /* close the database */
  if(!tcfdbclose(fdb)){
    ecode = tcfdbecode(fdb);
    fprintf(stderr, "close error: %s\n", tcfdberrmsg(ecode));
  }

  /* delete the object */
  tcfdbdel(fdb);

  return 0;
}
```

This can be done so via these two commands:

```
$	gcc -c -I. -I.. -ansi -Wall -pedantic -fsigned-char -O2 test.c
$	LD_RUN_PATH=/lib:/usr/lib:/home/guyinatuxedo/lib:/usr/local/lib:.:.. gcc -I. -I.. -ansi -Wall -pedantic -fsigned-char -O2 -o test test.o  -L. -L.. -ltokyocabinet -lz -lbz2 -lpthread -lm -lc
```

## API Calls