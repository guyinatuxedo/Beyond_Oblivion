#include "../Db/dbapi.h"

int main(int argc, char **argv) {
  void *db, *rec0, *rec1;
  wg_int int0, string0, string1;

  // Make/attach the db
  db = wg_attach_database("leak-db-0", 250000);

  // Make the records
  rec0 = wg_create_record(db, 10);
  rec1 = wg_create_record(db, 10);

  // Make the values to insert into the records
  int0 = wg_encode_int(db, 0x50);

  string0 = wg_encode_str(db, "Somewhere in the stratosphere", NULL);
  string1 = wg_encode_str(db, "have to think twice before you", NULL);

  // Store the values in the records
  wg_set_field(db, rec0, 0, int0);
  wg_set_field(db, rec0, 1, int0);
  wg_set_field(db, rec0, 2, int0);
  wg_set_field(db, rec0, 3, string0);

  wg_set_field(db, rec1, 0, int0);
  wg_set_field(db, rec1, 1, int0);
  wg_set_field(db, rec1, 2, int0);
  wg_set_field(db, rec1, 3, string1);

  // Dump the records
  wg_dump(db, "leak-db-file-0");

  wg_delete_database("leak-db-0");

  return 0;
}


