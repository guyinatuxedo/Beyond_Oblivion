# HashDB Operations

So this file will cover how the database will actually perform these operations:
```
Database Open
Record Insertion
Record Deletion
Database Close
```

## Database Open

So this will document the process of opening up a database, using the `tchdbopen` function. The function arguments it takes are the `TCHDB` object ptr (the object which actually models the database), a string ptr to the filepath, and an integer reprenting what type of mode you want to open it in (`tchdb.c`):

```
/* Open a database file and connect a hash database object. */
bool tchdbopen(TCHDB *hdb, const char *path, int omode){
  assert(hdb && path);
  if(!HDBLOCKMETHOD(hdb, true)) return false;
  if(hdb->fd >= 0){
    tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
    HDBUNLOCKMETHOD(hdb);
    return false;
  }
  char *rpath = tcrealpath(path);
  if(!rpath){
    int ecode = TCEOPEN;
    switch(errno){
      case EACCES: ecode = TCENOPERM; break;
      case ENOENT: ecode = TCENOFILE; break;
      case ENOTDIR: ecode = TCENOFILE; break;
    }
    tchdbsetecode(hdb, ecode, __FILE__, __LINE__, __func__);
    HDBUNLOCKMETHOD(hdb);
    return false;
  }
  if(!tcpathlock(rpath)){
    tchdbsetecode(hdb, TCETHREAD, __FILE__, __LINE__, __func__);
    TCFREE(rpath);
    HDBUNLOCKMETHOD(hdb);
    return false;
  }
  bool rv = tchdbopenimpl(hdb, path, omode);
  if(rv){
    hdb->rpath = rpath;
  } else {
    tcpathunlock(rpath);
    TCFREE(rpath);
  }
  HDBUNLOCKMETHOD(hdb);
  return rv;
}
```

So looking at this function, we see that it will first try to synthesize a full filepath with the `tcrealpath` (that way it could be passed a relative filepath, and synthesize it to a full filepath to use). Then it will lock the file using `tcpathlock` (not going into the file locking functionallity it has). Then it will call the `tchdbopenimpl` function, which primarily handles the database opening logic.

So for the `tchdbopenimpl` function (`tchdb.c`), I'm going to go through it with the assumption that I have the `HDBOWRITER | HDBOCREAT` flags, since those are the ones in the examples I've seen.

So first off in this funciton, it opens a file descriptor to the file:

```
  int fd = open(path, mode, HDBFILEMODE);
```

Next off, it appears to populate the values from the database header (see the database format description for more info on what that entails). So for this, there appears to be several different possibilities, depending on if the database file already exists or not before this process started, which we see, it tries to detect using `fstat`:

```
  struct stat sbuf;
  if(fstat(fd, &sbuf) == -1 || !S_ISREG(sbuf.st_mode)){
    tchdbsetecode(hdb, TCESTAT, __FILE__, __LINE__, __func__);
    close(fd);
    return false;
  }
  char hbuf[HDBHEADSIZ];
  if((omode & HDBOWRITER) && sbuf.st_size < 1){
```

So if the database file did not exist when it initially opened it, we see that it will populate values of `hdb` (the `TCHDB` object representing the database) with the default values, use the `tchdbdumpmeta` function to actually write the header data to a buffer, and then use `tcwrite` to write the data to the output file:

```
  if((omode & HDBOWRITER) && sbuf.st_size < 1){
    hdb->flags = 0;
    hdb->rnum = 0;
    uint32_t fbpmax = 1 << hdb->fpow;
    uint32_t fbpsiz = HDBFBPBSIZ + fbpmax * HDBFBPESIZ;
    int besiz = (hdb->opts & HDBTLARGE) ? sizeof(int64_t) : sizeof(int32_t);
    hdb->align = 1 << hdb->apow;
    hdb->fsiz = HDBHEADSIZ + besiz * hdb->bnum + fbpsiz;
    hdb->fsiz += tchdbpadsize(hdb, hdb->fsiz);
    hdb->frec = hdb->fsiz;
    tchdbdumpmeta(hdb, hbuf);
    bool err = false;
    if(!tcwrite(fd, hbuf, HDBHEADSIZ)) err = true;
    char pbuf[HDBIOBUFSIZ];
    memset(pbuf, 0, HDBIOBUFSIZ);
    uint64_t psiz = hdb->fsiz - HDBHEADSIZ;
    while(psiz > 0){
      if(psiz > HDBIOBUFSIZ){
        if(!tcwrite(fd, pbuf, HDBIOBUFSIZ)) err = true;
        psiz -= HDBIOBUFSIZ;
      } else {
        if(!tcwrite(fd, pbuf, psiz)) err = true;
        psiz = 0;
      }
    }
    if(err){
      tchdbsetecode(hdb, TCEWRITE, __FILE__, __LINE__, __func__);
      close(fd);
      return false;
    }
    sbuf.st_size = hdb->fsiz;
  }
```

Now proceeding that, regardless of wether or not the file existed, it will attempt to read the header information, and populate the corresponding `hdb` fields using `tchdbloadmeta`:

```
  if(lseek(fd, 0, SEEK_SET) == -1){
    tchdbsetecode(hdb, TCESEEK, __FILE__, __LINE__, __func__);
    close(fd);
    return false;
  }
  if(!tcread(fd, hbuf, HDBHEADSIZ)){
    tchdbsetecode(hdb, TCEREAD, __FILE__, __LINE__, __func__);
    close(fd);
    return false;
  }
  int type = hdb->type;
  tchdbloadmeta(hdb, hbuf);
```

Proceeding that, we see it will actually allocate the memory, which models the database. This is done via an `mmap` call. The size of this database is either the size of the header plus the size of the bucket array (number of buckets times the size of a bucket):

```
  int besiz = (hdb->opts & HDBTLARGE) ? sizeof(int64_t) : sizeof(int32_t);
  size_t msiz = HDBHEADSIZ + hdb->bnum * besiz;
```

or the size of the extra mapped memory (`67108864`):

```
hdb->xmsiz = HDBDEFXMSIZ;
```

and

```
#define HDBDEFXMSIZ    (64LL<<20)        // default size of the extra mapped memory
```

whichever is larger (in the runs I've seen, it's `0x4000000`, so `xmsiz`):

```
  size_t xmsiz = (hdb->xmsiz > msiz) ? hdb->xmsiz : msiz;
  if(!(omode & HDBOWRITER) && xmsiz > hdb->fsiz) xmsiz = hdb->fsiz;
  void *map = mmap(0, xmsiz, PROT_READ | ((omode & HDBOWRITER) ? PROT_WRITE : 0),
                   MAP_SHARED, fd, 0);
```

We see that this map is actually saved:

```
  hdb->map = map;
  hdb->msiz = msiz;
```

The bucket arrays actually are pointers within this region:

```
  if(hdb->opts & HDBTLARGE){
    hdb->ba32 = NULL;
    hdb->ba64 = (uint64_t *)((char *)map + HDBHEADSIZ);
  } else {
    hdb->ba32 = (uint32_t *)((char *)map + HDBHEADSIZ);
    hdb->ba64 = NULL;
  }
```

Finishing off, we see that it will attempt to load the free block pool using the `tchdbloadfbp` function, and then sort them. Proceeding that it will set the flag on the `hdb`, meaning that it is open:

```
    tchdbsetflag(hdb, HDBFOPEN, true);
```

## Record Lookup

So there are multiple function calls that will lead to record retrieval, this will be from the `tchdbget` function. 


```
0.)	Hash the key
```



So starting off, it will hash the key, to get the primary and secondary hashes (secondary hash is used to uniquely identify keys where hash collosions occur witht he primary hash):

```

```

It will then use the primary hash to lookup the bucket offset in the bucket array. It will then attempt to read the data at the offset, as a record.

Proceeding that, it will check if the secondary hashes are equivalent. If not, it will check if there is a child for that hash, and iterate down the bucket tree accordingly. Once it finds a record with a corresponding secondary hash, it will compare the keys to see if they are equivalent. If they are, it will scan the value of the record and return it.

## Record Insertion

So for starting off, it will hasht the key just like for record insertion. It will then attempt to look up the offset to the bucket, in the bucket array. Now, this is where the insertion process will fork off into different methods. This depends on what it finds in the bucket, and here are the conditions:

```
0.)	There is no bucket offet
1.) There are records in the bucket, but none with the same key
2.) There are records in the bucket, with the same key
```

#### No Bucket Offset

So this condition will occur when we are inserting a record, that the bucket doesn't actually have any records, so the bucket hasn't been made yet. For this method, it will attempt

#### Records in bucket, with different key

#### Records in bucket, with same key
