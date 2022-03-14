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

So there are multiple function calls that will lead to record retrieval, this will be from the `tchdbget` function. This function will effectively query the database for a record, by a key that is passed in through the arguments. The first argument `hdb` is the database object which models the db, the second argument `kbuf` is a pointer to the string that is the key, the third argument `ksiz` is the size of the key, and the final argument `sp` is a pointer to the size of the returned data (this function returns a ptr to the record that was looked up):

```
/* Retrieve a record in a hash database object. */
void *tchdbget(TCHDB *hdb, const void *kbuf, int ksiz, int *sp){
  assert(hdb && kbuf && ksiz >= 0 && sp);
  if(!HDBLOCKMETHOD(hdb, false)) return NULL;
  uint8_t hash;
  uint64_t bidx = tchdbbidx(hdb, kbuf, ksiz, &hash);
  if(hdb->fd < 0){
    tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
    HDBUNLOCKMETHOD(hdb);
    return NULL;
  }
  if(hdb->async && !tchdbflushdrp(hdb)){
    HDBUNLOCKMETHOD(hdb);
    return NULL;
  }
  if(!HDBLOCKRECORD(hdb, bidx, false)){
    HDBUNLOCKMETHOD(hdb);
    return false;
  }
  char *rv = tchdbgetimpl(hdb, kbuf, ksiz, bidx, hash, sp);
  HDBUNLOCKRECORD(hdb, bidx);
  HDBUNLOCKMETHOD(hdb);
  return rv;
}
```

So starting off, it will use the `tchdbbidx` to actually generate the first and secondary hashes (secondary hashes being used to uniquely identify keys when hash collisions occur with first keys):

```
/* Get the bucket index of a record.
   `hdb' specifies the hash database object.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   `hp' specifies the pointer to the variable into which the second hash value is assigned.
   The return value is the bucket index. */
static uint64_t tchdbbidx(TCHDB *hdb, const char *kbuf, int ksiz, uint8_t *hp){
  assert(hdb && kbuf && ksiz >= 0 && hp);
  uint64_t idx = 19780211;
  uint32_t hash = 751;
  const char *rp = kbuf + ksiz;
  while(ksiz--){
    idx = idx * 37 + *(uint8_t *)kbuf++;
    hash = (hash * 31) ^ *(uint8_t *)--rp;
  }
  *hp = hash;
  return idx % hdb->bnum;
}
```

So for finishing up the `tchdbget` function, we see that it locks the record, than actually executes the record lookup procedure with the `tchdbgetimpl` function.

So starting off, we see that the `tchdbgetimpl` function will get the offset to the bucket, which would hold the record, using the `tchdbgetbucket` function:

```
  off_t off = tchdbgetbucket(hdb, bidx);
```

And we see, it effectively just grabs the offset from `hdb->ba<64/32>`, which from the openign section is a an array/ptr to the map, which is actually the mapped memory which comprises the database. So it effectively just gets the offset to the bucket, from the bucket array:

```
/* Get the offset of the record of a bucket element.
   `hdb' specifies the hash database object.
   `bidx' specifies the index of the bucket.
   The return value is the offset of the record. */
static off_t tchdbgetbucket(TCHDB *hdb, uint64_t bidx){
  assert(hdb && bidx >= 0);
  if(hdb->ba64){
    uint64_t llnum = hdb->ba64[bidx];
    return TCITOHLL(llnum) << hdb->apow;
  }
  uint32_t lnum = hdb->ba32[bidx];
  return (off_t)TCITOHL(lnum) << hdb->apow;
}
```

Following that in the `tchdbgetimpl` function, it will enter into a while loop to try and find the record within the bucket. How it does this is this. It will start at the record at the beginning of the bucket, via setting the current record offset to the one from the bucket array (if there is no offset because there is no bucket, it just exits out). It will attempt to read that record. Then it will check if the secondary hashes match. if they do not match, then depending on if the secondary hash is less than/greater than, it will set it's offset to the left/right child and move on to the next iteration of the while loop, to iterate down the tree. After it has found a record with a matching secondary hash, it will then compare the actual keys with the `tcreckeycmp` function (effectively wraps `memcmp`), and a similar thing if the keys don't match, iterate down the left/right children of the bucket tree for that node. We see al of this happen here:

```
  TCHREC rec;
  char rbuf[HDBIOBUFSIZ];
  while(off > 0){
    rec.off = off;
    if(!tchdbreadrec(hdb, &rec, rbuf)) return NULL;
    if(hash > rec.hash){
      off = rec.left;
    } else if(hash < rec.hash){
      off = rec.right;
    } else {
      if(!rec.kbuf && !tchdbreadrecbody(hdb, &rec)) return NULL;
      int kcmp = tcreckeycmp(kbuf, ksiz, rec.kbuf, rec.ksiz);
      if(kcmp > 0){
        off = rec.left;
        TCFREE(rec.bbuf);
        rec.kbuf = NULL;
        rec.bbuf = NULL;
      } else if(kcmp < 0){
        off = rec.right;
        TCFREE(rec.bbuf);
        rec.kbuf = NULL;
        rec.bbuf = NULL;
      } else {
```

Now assuming both the secondary hash and they key comparisons are equivalent, it will then actually read the record.


So starting off, it will hash the key, to get the primary and secondary hashes (secondary hash is used to uniquely identify keys where hash collosions occur witht he primary hash):

```
/* Get the offset of the record of a bucket element.
   `hdb' specifies the hash database object.
   `bidx' specifies the index of the bucket.
   The return value is the offset of the record. */
static off_t tchdbgetbucket(TCHDB *hdb, uint64_t bidx){
  assert(hdb && bidx >= 0);
  if(hdb->ba64){
    uint64_t llnum = hdb->ba64[bidx];
    return TCITOHLL(llnum) << hdb->apow;
  }
  uint32_t lnum = hdb->ba32[bidx];
  return (off_t)TCITOHL(lnum) << hdb->apow;
}

```

It will then use the primary hash to lookup the bucket offset in the bucket array. It will then attempt to read the data at the offset, as a record. We see it does this with the `tchdbreadrecbody` function call here:

```
      } else {
        if(!rec.vbuf && !tchdbreadrecbody(hdb, &rec)) return NULL;
        if(hdb->zmode){
          int zsiz;
```

Now looking at the `tchdbreadrecbody` function, we see this:

```
/* Read the body of a record from the file.
   `hdb' specifies the hash database object.
   `rec' specifies the record object.
   The return value is true if successful, else, it is false. */
static bool tchdbreadrecbody(TCHDB *hdb, TCHREC *rec){
  assert(hdb && rec);
  int32_t bsiz = rec->ksiz + rec->vsiz;
  TCMALLOC(rec->bbuf, bsiz + 1);
  if(!tchdbseekread(hdb, rec->boff, rec->bbuf, bsiz)) return false;
  rec->kbuf = rec->bbuf;
  rec->vbuf = rec->bbuf + rec->ksiz;
  return true;
}
```

Now before getting into this, it will help to look at the `TCHREC` struct:

```
typedef struct {                         // type of structure for a record
  uint64_t off;                          // offset of the record
  uint32_t rsiz;                         // size of the whole record
  uint8_t magic;                         // magic number
  uint8_t hash;                          // second hash value
  uint64_t left;                         // offset of the left child record
  uint64_t right;                        // offset of the right child record
  uint32_t ksiz;                         // size of the key
  uint32_t vsiz;                         // size of the value
  uint16_t psiz;                         // size of the padding
  const char *kbuf;                      // pointer to the key
  const char *vbuf;                      // pointer to the value
  uint64_t boff;                         // offset of the body
  char *bbuf;                            // buffer of the body
} TCHREC;
```

So what is going here is this. The `tchdbreadrecbody` function effectively just wraps `tchdbseekread`. The `boff` is a `uint64` offset to the body of the record. We also see it will allocate a buffer into `rec->bbuf` to store the record into, and it sets `rec->vbuf` as the memory scanned into `rec->bbuf`, but moves the pointer up by the size of the key so it points to just the body of the record. Looking at the `tchdbseekread`, I believe the functionallity here we will see is it just memecpy's the data. We see there is some other functionallity, in the event the read goes beyond the total size of the allocated space:

```
/* Seek and read data from a file.
   `hdb' specifies the hash database object.
   `off' specifies the offset of the region to seek.
   `buf' specifies the buffer to store into.
   `size' specifies the size of the buffer.
   The return value is true if successful, else, it is false. */
static bool tchdbseekread(TCHDB *hdb, off_t off, void *buf, size_t size){
  assert(hdb && off >= 0 && buf && size >= 0);
  if(off + size <= hdb->xmsiz){
    memcpy(buf, hdb->map + off, size);
    return true;
  }
  if(!TCUBCACHE && off < hdb->xmsiz){
    int head = hdb->xmsiz - off;
    memcpy(buf, hdb->map + off, head);
    off += head;
    buf = (char *)buf + head;
    size -= head;
  }
  while(true){
    int rb = pread(hdb->fd, buf, size, off);
    if(rb >= size){
      break;
    } else if(rb > 0){
      buf = (char *)buf + rb;
      size -= rb;
      off += rb;
    } else if(rb == -1){
      if(errno != EINTR){
        tchdbsetecode(hdb, TCEREAD, __FILE__, __LINE__, __func__);
        return false;
      }
    } else {
      if(size > 0){
        tchdbsetecode(hdb, TCEREAD, __FILE__, __LINE__, __func__);
        return false;
      }
    }
  }
  return true;
}

```

So getting back to the `tchdbgetimpl` funciton, we understand that the `tchdbgetimpl` function effectively just sets the TCHREC element `rec->vbuf` to a ptr to the body of the record we queried for. Some other functionallity for things like compression which we aren't looking for, then it calls the `TCMEMDUP` macro, which will just allocate a new block of memory, and memcpy the record body to it, then return a ptr to that:

```
        *sp = rec.vsiz;
        char *rv;
        TCMEMDUP(rv, rec.vbuf, rec.vsiz);
        return rv;
```

## Record Insertion

So this will cover the process or record insertion, with the `tchdbput` function, which take these arguments:

```
hdb     -   The TCHDB object ptr, which models the database
kbuf    -   ptr to the key, for the record to be inserted
ksiz    -   size of the key for the record to be inserted
vbuf    -   ptr to the value, for the record to be inserted
vsiz    -   size of the value for the record to be inserted
```

So starting off, it will hash the key with `tchdbbidx`. Also some of this stuff is repeated from the record selection section, so I won't recover it here. There appears to be a lot of extra functionallity for dealing with things like compression. We see that it will attempt to lock the record, then call the `tchdbputimpl` function to actually handle record insertion. For this, it appears the final argument is an enum to specify behavior, we have `HDBPDOVER` here:

```
    bool rv = tchdbputimpl(hdb, kbuf, ksiz, bidx, hash, zbuf, vsiz, HDBPDOVER);
```

Now in the `tchdbputimpl`, it will attempt to search for the record, with the same process that it uses for selection. Now depending if it finds the record or not, different things can happen. I will cover these different conditions:

```
record insertion, the record does not exist
record insertion, the record exists
```

#### Record Insertion, record does not exist

So this is for when it inserts a record, that does not already exist. So the first thing that we see is that it will attempt to generate a size value for the record. It will do this first via seeing if it's 32/64 bit for the record header, then increment the length too based upon the size of the key/value:

```
  rec.rsiz = hdb->ba64 ? sizeof(uint8_t) * 2 + sizeof(uint64_t) * 2 + sizeof(uint16_t) :
    sizeof(uint8_t) * 2 + sizeof(uint32_t) * 2 + sizeof(uint16_t);
  if(ksiz < (1U << 7)){
    rec.rsiz += 1;
  } else if(ksiz < (1U << 14)){
    rec.rsiz += 2;
  } else if(ksiz < (1U << 21)){
    rec.rsiz += 3;
  } else if(ksiz < (1U << 28)){
    rec.rsiz += 4;
  } else {
    rec.rsiz += 5;
  }
  if(vsiz < (1U << 7)){
    rec.rsiz += 1;
  } else if(vsiz < (1U << 14)){
    rec.rsiz += 2;
  } else if(vsiz < (1U << 21)){
    rec.rsiz += 3;
  } else if(vsiz < (1U << 28)){
    rec.rsiz += 4;
  } else {
    rec.rsiz += 5;
  }
```

Proceeding that, it will attempt to find a spot for the record to go using the `tchdbfbpsearch` function:

```
  if(!tchdbfbpsearch(hdb, &rec)){
    HDBUNLOCKDB(hdb);
    return false;
  }
```

So looking at the `tchdbfbpsearch` function, we see that it will actually set the offset to the record it gets passed to the offset it finds. Now the first thing it will check is if it has any free blocks available (`hdb->fbpnum`):

```
/* Search the free block pool for the minimum region.
   `hdb' specifies the hash database object.
   `rec' specifies the record object to be stored.
   The return value is true if successful, else, it is false. */
static bool tchdbfbpsearch(TCHDB *hdb, TCHREC *rec){
  assert(hdb && rec);
  TCDODEBUG(hdb->cnt_searchfbp++);
  if(hdb->fbpnum < 1){
    rec->off = hdb->fsiz;
    rec->rsiz = 0;
    return true;
  }
```

Now much like a heap implementation, this database implementation has a free list of some sort, which deleted records get inserted into, so it can then reuse the space. I'm not going to get into the free list implementation here. We see that if there are no free blocks available, it just puts the offset to the size of the database file, effectively just allocating additional space to store it.

Proceeding that, we see it calls the `tchdbwriterec` function to actually handle the process of writing the record:

```
  rec.hash = hash;
  rec.left = 0;
  rec.right = 0;
  rec.ksiz = ksiz;
  rec.vsiz = vsiz;
  rec.psiz = 0;
  rec.kbuf = kbuf;
  rec.vbuf = vbuf;
  if(!tchdbwriterec(hdb, &rec, bidx, entoff)){
    HDBUNLOCKDB(hdb);
    return false;
  }
```

So in the `tchdbwriterec` we can see exactly where it actually writes the record to memory. In here, we see that it first constructs the record in memory (either stack or heap), then writes it to the actual database memory:

```
  char *rbuf;
  if(bsiz <= HDBIOBUFSIZ){
    rbuf = stack;
  } else {
    TCMALLOC(rbuf, bsiz);
  }
  char *wp = rbuf;
```

Next up in `tchdbwriterec`, we can see it writes the magic value, secondary hash, and the left/right children (if then for 64/32 bit, because different sized values). See the hashdb description for what these values mean:

```
  *(uint8_t *)(wp++) = HDBMAGICREC;
  *(uint8_t *)(wp++) = rec->hash;
  if(hdb->ba64){
    uint64_t llnum;
    llnum = rec->left >> hdb->apow;
    llnum = TCHTOILL(llnum);
    memcpy(wp, &llnum, sizeof(llnum));
    wp += sizeof(llnum);
    llnum = rec->right >> hdb->apow;
    llnum = TCHTOILL(llnum);
    memcpy(wp, &llnum, sizeof(llnum));
    wp += sizeof(llnum);
  } else {
    uint32_t lnum;
    lnum = rec->left >> hdb->apow;
    lnum = TCHTOIL(lnum);
    memcpy(wp, &lnum, sizeof(lnum));
    wp += sizeof(lnum);
    lnum = rec->right >> hdb->apow;
    lnum = TCHTOIL(lnum);
    memcpy(wp, &lnum, sizeof(lnum));
    wp += sizeof(lnum);
  }
```

Next up, we can see it writes the key and value sizes to the record. Now in the record header, these go after the padding size, so it saves the current position in `pwp` to write that later on:

```
  uint16_t snum;
  char *pwp = wp;
  wp += sizeof(snum);
  int step;
  TCSETVNUMBUF(step, wp, rec->ksiz);
  wp += step;
  TCSETVNUMBUF(step, wp, rec->vsiz);
  wp += step;
```

Next up, we see a block of code, which is responsible for deciding how much padding will be needed. Towards the end of this, we see that it actually writes that to the record header:

```
  if(rec->rsiz < 1){
    uint16_t psiz = tchdbpadsize(hdb, hdb->fsiz + rsiz);
    rec->rsiz = rsiz + psiz;
    rec->psiz = psiz;
    finc = rec->rsiz;
  } else if(rsiz > rec->rsiz){
    if(rbuf != stack) TCFREE(rbuf);
    if(!HDBLOCKDB(hdb)) return false;
    if(tchdbfbpsplice(hdb, rec, rsiz)){
      TCDODEBUG(hdb->cnt_splicefbp++);
      bool rv = tchdbwriterec(hdb, rec, bidx, entoff);
      HDBUNLOCKDB(hdb);
      return rv;
    }
    TCDODEBUG(hdb->cnt_moverec++);
    if(!tchdbwritefb(hdb, rec->off, rec->rsiz)){
      HDBUNLOCKDB(hdb);
      return false;
    }
    tchdbfbpinsert(hdb, rec->off, rec->rsiz);
    rec->rsiz = rsiz;
    if(!tchdbfbpsearch(hdb, rec)){
      HDBUNLOCKDB(hdb);
      return false;
    }
    bool rv = tchdbwriterec(hdb, rec, bidx, entoff);
    HDBUNLOCKDB(hdb);
    return rv;
  } else {
    TCDODEBUG(hdb->cnt_reuserec++);
    uint32_t psiz = rec->rsiz - rsiz;
    if(psiz > UINT16_MAX){
      TCDODEBUG(hdb->cnt_dividefbp++);
      psiz = tchdbpadsize(hdb, rec->off + rsiz);
      uint64_t noff = rec->off + rsiz + psiz;
      uint32_t nsiz = rec->rsiz - rsiz - psiz;
      rec->rsiz = noff - rec->off;
      rec->psiz = psiz;
      if(!tchdbwritefb(hdb, noff, nsiz)){
        if(rbuf != stack) TCFREE(rbuf);
        return false;
      }
      if(!HDBLOCKDB(hdb)){
        if(rbuf != stack) TCFREE(rbuf);
        return false;
      }
      tchdbfbpinsert(hdb, noff, nsiz);
      HDBUNLOCKDB(hdb);
    }
    rec->psiz = psiz;
  }
  snum = rec->psiz;
  snum = TCHTOIS(snum);
  memcpy(pwp, &snum, sizeof(snum));
```

Proceeding that, we see that it will actually write the key/value/padding (null bytes for padding) to the record it constructed:

```
  rsiz = rec->rsiz;
  rsiz -= hsiz;
  memcpy(wp, rec->kbuf, rec->ksiz);
  wp += rec->ksiz;
  rsiz -= rec->ksiz;
  memcpy(wp, rec->vbuf, rec->vsiz);
  wp += rec->vsiz;
  rsiz -= rec->vsiz;
  memset(wp, 0, rsiz);
```

Then next up, it will actually write the constructed record to the database. Now it will do this using `rbuf` (points to the beginning of the record), since we see back further up, that `wp` is initialized to `rbuf`, that way it could just increment `wp` to continue building the record.:

```
  if(!tchdbseekwrite(hdb, rec->off, rbuf, rec->rsiz)){
    if(rbuf != stack) TCFREE(rbuf);
    return false;
  }
```

Now looking at the `tchdbseekwrite` function, in the simple case, it appears to just use `memcpy` to write to the mapped memory, at the offset we provide:

```
/* Seek and write data into a file.
   `hdb' specifies the hash database object.
   `off' specifies the offset of the region to seek.
   `buf' specifies the buffer to store into.
   `size' specifies the size of the buffer.
   The return value is true if successful, else, it is false. */
static bool tchdbseekwrite(TCHDB *hdb, off_t off, const void *buf, size_t size){
  assert(hdb && off >= 0 && buf && size >= 0);
  if(hdb->tran && !tchdbwalwrite(hdb, off, size)) return false;
  off_t end = off + size;
  if(end <= hdb->xmsiz){
    if(end >= hdb->fsiz && end >= hdb->xfsiz){
      uint64_t xfsiz = end + HDBXFSIZINC;
      if(ftruncate(hdb->fd, xfsiz) == -1){
        tchdbsetecode(hdb, TCETRUNC, __FILE__, __LINE__, __func__);
        return false;
      }
      hdb->xfsiz = xfsiz;
    }
    memcpy(hdb->map + off, buf, size);
    return true;
  }
```

Now getting back to `tchdbwriterec`, we see that if it used malloc'd space for `rbuf`, it frees it:

```
  if(rbuf != stack) TCFREE(rbuf);
```

Proceeding that, it will record the offset to the record. This will either be in the bucket array if this record is the first one in the bucket (when `entoff` is `0`, and it does this via `tchdbsetbucket`), or it will write it's offset to the left/right child of it's parent in the bucket, which it finds as it is iterating through the bucket in the while loop mentioned above. Just because the record does not exist, doesn't mean the bucket it belongs to does not exist:

```
  if(entoff > 0){
    if(hdb->ba64){
      uint64_t llnum = rec->off >> hdb->apow;
      llnum = TCHTOILL(llnum);
      if(!tchdbseekwrite(hdb, entoff, &llnum, sizeof(uint64_t))) return false;
    } else {
      uint32_t lnum = rec->off >> hdb->apow;
      lnum = TCHTOIL(lnum);
      if(!tchdbseekwrite(hdb, entoff, &lnum, sizeof(uint32_t))) return false;
    }
  } else {
    tchdbsetbucket(hdb, bidx, rec->off);
  }
```

Now getting back to `tchdbputimpl`, the final thing we see it does, is increment the record count, and write it the mapped database memory before returning `true`:

```
  hdb->rnum++;
  uint64_t llnum = hdb->rnum;
  llnum = TCHTOILL(llnum);
  memcpy(hdb->map + HDBRNUMOFF, &llnum, sizeof(llnum));
  HDBUNLOCKDB(hdb);
  return true;
}
```


#### Record Insertion, record exists

So for this process, it will search for the record the same way selection does (find the bucket with the primary hash, search through the bucket using the secondary hash / key). Once it finds it though, there are several different behaviors defined depending on the `dmode` argument. In our case, it is `HDBPDOVER`, which appears to be handled by the default case:

```
          default:
            break;
        }
        TCFREE(rec.bbuf);
        rec.ksiz = ksiz;
        rec.vsiz = vsiz;
        rec.kbuf = kbuf;
        rec.vbuf = vbuf;
        return tchdbwriterec(hdb, &rec, bidx, entoff);
```

It appears to simply write the record, ontop of the existing one at that offset.

## Database Close

So this will cover the database close process using the `tchdbclose` function, which takes a single argument being a ptr to the `TCHDB` object to close:

```
/* Close a database object. */
bool tchdbclose(TCHDB *hdb){
  assert(hdb);
  if(!HDBLOCKMETHOD(hdb, true)) return false;
  if(hdb->fd < 0){
    tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
    HDBUNLOCKMETHOD(hdb);
    return false;
  }
  bool rv = tchdbcloseimpl(hdb);
  tcpathunlock(hdb->rpath);
  TCFREE(hdb->rpath);
  hdb->rpath = NULL;
  HDBUNLOCKMETHOD(hdb);
  return rv;
}
```

Looking at this function, we can see it effectively just wraps `tchdbcloseimpl`:

```
/* Close a hash database object.
   `hdb' specifies the hash database object.
   If successful, the return value is true, else, it is false. */
static bool tchdbcloseimpl(TCHDB *hdb){
  assert(hdb);
  bool err = false;
  if(hdb->recc){
    tcmdbdel(hdb->recc);
    hdb->recc = NULL;
  }
  if(hdb->omode & HDBOWRITER){
    if(!tchdbflushdrp(hdb)) err = true;
    if(hdb->tran) hdb->fbpnum = 0;
    if(!tchdbsavefbp(hdb)) err = true;
    TCFREE(hdb->fbpool);
    tchdbsetflag(hdb, HDBFOPEN, false);
  }
  if((hdb->omode & HDBOWRITER) && !tchdbmemsync(hdb, false)) err = true;
  size_t xmsiz = (hdb->xmsiz > hdb->msiz) ? hdb->xmsiz : hdb->msiz;
  if(!(hdb->omode & HDBOWRITER) && xmsiz > hdb->fsiz) xmsiz = hdb->fsiz;
  if(munmap(hdb->map, xmsiz) == -1){
    tchdbsetecode(hdb, TCEMMAP, __FILE__, __LINE__, __func__);
    err = true;
  }
  hdb->map = NULL;
  if((hdb->omode & HDBOWRITER) && ftruncate(hdb->fd, hdb->fsiz) == -1){
    tchdbsetecode(hdb, TCETRUNC, __FILE__, __LINE__, __func__);
    err = true;
  }
  if(hdb->tran){
    if(!tchdbwalrestore(hdb, hdb->path)) err = true;
    hdb->tran = false;
  }
  if(hdb->walfd >= 0){
    if(close(hdb->walfd) == -1){
      tchdbsetecode(hdb, TCECLOSE, __FILE__, __LINE__, __func__);
      err = true;
    }
    if(!hdb->fatal && !tchdbwalremove(hdb, hdb->path)) err = true;
  }
  if(close(hdb->fd) == -1){
    tchdbsetecode(hdb, TCECLOSE, __FILE__, __LINE__, __func__);
    err = true;
  }
  TCFREE(hdb->path);
  hdb->path = NULL;
  hdb->fd = -1;
  return !err;
}
```

So this function calls various functions and runs checks, to ensure the database closing process suceeded. Starting off, if the database was in `HDBOWRITER` mode, it will call `tchdbflushdrp` to flush the delayed record pool (unsure what that is), the `tchdbsavefbp` to close out/save the free block pool information, and `tchdbsetflag` to mark the database as closed.

Proceeding that, if the database was in `HDBOWRITER`, it will call `tchdbmemsync` to actually write the data to a file, since it could have changed:

```
/* Synchronize updating contents on memory of a hash database object. */
bool tchdbmemsync(TCHDB *hdb, bool phys){
  assert(hdb);
  if(hdb->fd < 0 || !(hdb->omode & HDBOWRITER)){
    tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
    return false;
  }
  bool err = false;
  char hbuf[HDBHEADSIZ];
  tchdbdumpmeta(hdb, hbuf);
  memcpy(hdb->map, hbuf, HDBOPAQUEOFF);
  if(phys){
    size_t xmsiz = (hdb->xmsiz > hdb->msiz) ? hdb->xmsiz : hdb->msiz;
    if(msync(hdb->map, xmsiz, MS_SYNC) == -1){
      tchdbsetecode(hdb, TCEMMAP, __FILE__, __LINE__, __func__);
      err = true;
    }
    if(fsync(hdb->fd) == -1){
      tchdbsetecode(hdb, TCESYNC, __FILE__, __LINE__, __func__);
      err = true;
    }
  }
  return !err;
}
```

We see here, it calls `msync` on the mapped data (which is the in memory database), for the size of the database, then calls `fsync` to actually dump the contents to the file.

Proceeding again with the `tchdbopenimpl` function, we see that it tries to unmap the mapped memory with `munmap`. After that, if the database was in `HDBOWRITER` mode, it will use `ftruncate` to truncate the file, to the desired length `fsiz`. After that, it will close the database file, free the filepath malloced data, and return.
