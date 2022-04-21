#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#ifndef _WIN32
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#endif

#ifdef _WIN32
#include <conio.h> // for _getch
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#include "../config-w32.h"
#else
#include "../config.h"
#endif
#include "../Db/dballoc.h"
#include "../Db/dbmem.h"
#include "../Db/dbdata.h"
#include "../Db/dbdump.h"
#include "../Db/dblog.h"
#include "../Db/dbquery.h"
#include "../Db/dbutil.h"
#include "../Db/dblock.h"
#include "../Db/dbjson.h"
#include "../Db/dbschema.h"
#ifdef USE_REASONER
#include "../Parser/dbparse.h"
#endif


#define FLAGS_LOGGING 0x2


/* ======= Private protos ================ */


int main(int argc, char **argv) {

  char *shmname = NULL;
  void *shmptr = NULL;
  void *rec, *stored_rec;
  wg_int int0, int1, int2, string, rec_encoded;
  int i, scan_to, len;
  gint shmsize;
  wg_int rlock = 0;
  wg_int wlock = 0;
  wg_int minsize, maxsize, err;
  wg_int flags = 0;



  shmsize = 0; /* 0 size causes default size to be used */

  err = wg_check_dump(NULL, "corrupted", &minsize, &maxsize);

  shmptr=wg_attach_memsegment(shmname, minsize, maxsize, 1,
    (flags & FLAGS_LOGGING), 0);

  // Import the database
  wg_import_dump(shmptr, "corrupted");

  rec = wg_get_first_record(shmptr);

  while (rec != NULL) {
	len = wg_get_record_len(shmptr, rec);

	if (len >= 3) {
		int0 = wg_get_field(shmptr, rec, 0);
		int1 = wg_get_field(shmptr, rec, 1);
		int2 = wg_get_field(shmptr, rec, 2);

		if ((int0 == int1) && (int1 == int2)) {
			if (int0 == 0x283) {
				string = wg_get_field(shmptr, rec, 3);
				printf("String: %s\n", (char *)wg_decode_str(shmptr, string));
			}
		}
	}

	rec = wg_get_next_record(shmptr, rec);
  }
}
