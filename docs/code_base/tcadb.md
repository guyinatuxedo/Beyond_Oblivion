# tcadb

So this document details the code base for the TokyoCabinet Abstract Database.

## New Database / Open Database

So the process of creating, and opening a new database is primarily done with the `tcadbnew`, and then the `tcadbopen` functions.

Looking at `tcadbnew`, we see that it effectively just allocates a new `tcadb *` struct on the heap, initializes a few values, and returns it.

```
/* Create an abstract database object. */
TCADB *tcadbnew(void){
  TCADB *adb;
  TCMALLOC(adb, sizeof(*adb));
  adb->omode = ADBOVOID;
  adb->mdb = NULL;
  adb->ndb = NULL;
  adb->hdb = NULL;
  adb->bdb = NULL;
  adb->fdb = NULL;
  adb->tdb = NULL;
  adb->capnum = -1;
  adb->capsiz = -1;
  adb->capcnt = 0;
  adb->cur = NULL;
  adb->skel = NULL;
  return adb;
}
```

Looking over the `tcadbopen`, we see that it effectively initializes a ton of values in the `tcadb *` struct. Might have to come back later to investigate this more thoroughly.

## Database Record Insertion

So looking at the db record insertion function `tcadbput2` (`tcadb.c`), we see that it is effectively just a wrapper for `tcadbput`:

```
/* Store a string record into an abstract object. */
bool tcadbput2(TCADB *adb, const char *kstr, const char *vstr){
  assert(adb && kstr && vstr);
  return tcadbput(adb, kstr, strlen(kstr), vstr, strlen(vstr));
}
```

Looking at the `tcadbput` (`tcadb.c`) function, we see that there is a switch statement which controls what insertion method is used. The value being evaluated by the switch statement is the `omode` field of the `adb`. Here is a quick chart detailing them:

```
ADBOMDB:	tcmdbcutfront()
ADBONDB:	tcndbcutfringe()
ADBOHDB:	tchdbput()
ADBOBDB:	tcbdbput()
ADBOFDB:	tcfdbput2()
ADBOTDB:	tctdbput2()
ADBOSKEL:	skel->put()
```

# tcfdput Insertion

So this part will cover how insertion works for `tcfdbput`. A `tcfdb`, is effectively a fixed-length database object. Also first off, looking at the `tcfdbput2` function, we see that it is effectively a wrapper for `tcfdput` (`tcfdb.c`):


```
/* Store a record with a decimal key into a fixed-length database object. */
bool tcfdbput2(TCFDB *fdb, const void *kbuf, int ksiz, const void *vbuf, int vsiz){
  assert(fdb && kbuf && ksiz >= 0 && vbuf && vsiz >= 0);
  return tcfdbput(fdb, tcfdbkeytoid(kbuf, ksiz), vbuf, vsiz);
}
```

So looking at the `tcfdbput` function, we see this:


```
/* Store a record into a fixed-length database object. */
bool tcfdbput(TCFDB *fdb, int64_t id, const void *vbuf, int vsiz){
  assert(fdb && vbuf && vsiz >= 0);
  if(!FDBLOCKMETHOD(fdb, id < 1)) return false;
  if(fdb->fd < 0 || !(fdb->omode & FDBOWRITER)){
    tcfdbsetecode(fdb, TCEINVALID, __FILE__, __LINE__, __func__);
    FDBUNLOCKMETHOD(fdb);
    return false;
  }
  if(id == FDBIDMIN){
    id = fdb->min;
  } else if(id == FDBIDPREV){
    id = fdb->min - 1;
  } else if(id == FDBIDMAX){
    id = fdb->max;
  } else if(id == FDBIDNEXT){
    id = fdb->max + 1;
  }
  if(id < 1 || id > fdb->limid){
    tcfdbsetecode(fdb, TCEINVALID, __FILE__, __LINE__, __func__);
    FDBUNLOCKMETHOD(fdb);
    return false;
  }
  if(!FDBLOCKRECORD(fdb, true, id)){
    FDBUNLOCKMETHOD(fdb);
    return false;
  }
  bool rv = tcfdbputimpl(fdb, id, vbuf, vsiz, FDBPDOVER);
  FDBUNLOCKRECORD(fdb, id);
  FDBUNLOCKMETHOD(fdb);
  return rv;
}
```

So starting off, it checks that `fdb` and `vbuf` are not null, and that `vsiz` is greater than or equal to `0x00`. Proceeding that, it does some checks on the `id` provide that it is withing the acceptable range. Proceeding that, it appears to use the `FDBLOCKRECORD` function to lock the record, so it can write to it. It appears the `tcfdbputimpl` is actually what handles record insertion. After that, it calls two functions to unlock the data.

So looking at the `tcfdbputimpl` (tcfdb.c) function, it appears that th0is is the meat of where the actual insertion logic is. Starting off, it checks that `fdb` is not null, and that `id` is greater than `0`. Also there is a check to see if `vsiz` is greater than the total database size, and if it is, set `vsis` to the total db size:

```
  assert(fdb && id > 0);
  if(vsiz > (int64_t)fdb->width) vsiz = fdb->width;
```

Next up, it attempts to generate a ptr to the record in which is being inserted. It does this via multiplying the id (-1) by the size of a record, and adding it to the base:

```
  unsigned char *rec = fdb->array + (id - 1) * (fdb->rsiz);
```

Next up we see that there appears to be a size check. The check is generated via seeing if `id` times the size of a record (`fdb->rsiz`) plus what looks like the size of some header (`FDBHEADSIZ`) is greater than `fdb->fsiz`, which I'm assuming is the size of the database. From this, we can tell that the records are meant to have a static size, and that the ids are probably meant to be generated in order (example, id 0, then 1, 2, 3...). Also this code appears to lead to a different code path, instead of just instantly returning an error. This is probably dealing with where the db has had an inserted record deleted, that it can then insert another record in it's place:

```
  uint64_t nsiz = FDBHEADSIZ + id * fdb->rsiz;
  if(nsiz > fdb->fsiz){
```


record sizes 256

insertion at `2203`




