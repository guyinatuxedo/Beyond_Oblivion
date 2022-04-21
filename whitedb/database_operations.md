## Opening a Database

So the process of opening up a database is done with the `wg_attach_database()` function (below from `tut7.c` example):

```
  void *db, *rec, *rec2, *rec3;
  wg_int enc;

  if(!(db = wg_attach_database("1000", 2000000)))
    exit(1); /* failed to attach */
```

Looking at the `wg_attach_database` (`whitedb.c` file):

```
void* wg_attach_database(const char* dbasename, gint size){
  void* shm = wg_attach_memsegment(dbasename, size, size, 1, 0, 0);
  CHECK_SEGMENT(shm)
  return shm;
}
```

It effectively calls the `wg_attach_memsegment`

## Record Creating

So in this database implementation, it will create with the `wg_create_record` function. Now records in this database are effectively tuples. In this function call, the `length` specifies the number of items in the tuple. The first argument `db` is a pointer to the object which models the database:

```
/* ------------ full record handling ---------------- */


void* wg_create_record(void* db, wg_int length) {
  void *rec = wg_create_raw_record(db, length);
  /* Index all the created NULL fields to ensure index consistency */
  if(rec) {
    if(wg_index_add_rec(db, rec) < -1)
      return NULL; /* index error */
  }
  return rec;
}
```

Looking at this function, it appears to effectively wrap `wg_create_raw_record`. Starting out, it checks that database with the `dbcheck` function:

```
#ifdef CHECK
  if (!dbcheck(db)) {
    show_data_error_nr(db,"wrong database pointer given to wg_create_record with length ",length);
    return 0;
  }
```

Which we see, the `dbcheck` function is just a call to `dbcheckh` and `dbmemsegh(db)`:

```
./Db/dballoc.h:#define dbcheck(db) dbcheckh(dbmemsegh(db)) /** check that correct db ptr */
```

So looking at these twp functions, we see that they are both effectively macros. The `dbmemsegh` appears to actually get the memory segment associated with the `db` struct: 

```
#define dbmemsegh(x) ((db_memsegment_header *)(((db_handle *) x)->db))
```

And then the actual check function, appears to check that it is not null, and that the start of the memory segment is equal to the constant `MEMSEGMENT_MAGIC_MARK`

```
./Db/dballoc.h:#define dbcheckh(dbh) (dbh!=NULL && *((gint32 *) dbh)==MEMSEGMENT_MAGIC_MARK) /** check that correct db ptr *
```

Which we see is this (`0x4973b223`):

```
./Db/dballoc.h:#define MEMSEGMENT_MAGIC_MARK 1232319011  /** enables to check that we really have db pointer */
```

Now getting back to the `wg_create_raw_record` function, we see thta it checks that the length is greater than `0x00`:

```
  if(length < 0) {
    show_data_error_nr(db, "invalid record length:",length);
    return 0;
  }
#endif
```

Next up, there appears to be some logging functionallity:

```
#ifdef USE_DBLOG
  /* Log first, modify shared memory next */
  if(dbmemsegh(db)->logging.active) {
    if(wg_log_create_record(db, length))
      return 0;
  }
#endif
```

Next up, it will actually allocate the record, using the database's custom heap implementation:

```
  offset=wg_alloc_gints(db,
                     &(dbmemsegh(db)->datarec_area_header),
                    length+RECORD_HEADER_GINTS);
```

Following that, it will initialize the values in the record, starting with the header. It will do it with the `dbstore` macro:

```
#define dbstore(db,offset,data) (*((gint*)(dbmemsegbytes(db)+(offset)))=data) /** store gint to address */
```

```
./Db/dbdata.h:#define RECORD_META_POS 1           /** metainfo, reserved for future use */
```

```
./Db/dbdata.h:#define RECORD_BACKLINKS_POS 2      /** backlinks structure offset */
```

```
./Reasoner/clterm.h:#define RECORD_HEADER_GINTS 3
```


```
  /* Init header */
  dbstore(db, offset+RECORD_META_POS*sizeof(gint), 0);
  dbstore(db, offset+RECORD_BACKLINKS_POS*sizeof(gint), 0);
  for(i=RECORD_HEADER_GINTS;i<length+RECORD_HEADER_GINTS;i++) {
    dbstore(db,offset+(i*(sizeof(gint))),0);
  }
```


```
#ifdef USE_DBLOG
  /* Append the created offset to log */
  if(dbmemsegh(db)->logging.active) {
    if(wg_log_encval(db, offset))
      return 0; /* journal error */
  }
#endif
```

```
  return offsettoptr(db,offset);
```

```
./Db/dballoc.h:#define offsettoptr(db,offset) ((void*)(dbmemsegbytes(db)+(offset))) /** give real address from offset */
```

## Value Encoding

So values are not directly deposited into records. They are first encoded into a special format, and then deposited. Here is an example, from one of the example source files:

```
  db = wg_attach_database("1000", 2000000);
  rec = wg_create_record(db, 10);
  rec2 = wg_create_record(db, 2);

  enc = wg_encode_int(db, 443);
  enc2 = wg_encode_str(db, "this is my string", NULL);

  wg_set_field(db, rec, 7, enc);
  wg_set_field(db, rec2, 0, enc2);
```

Here are some of the functions that do that

```
integers 		-	wg_encode_int
strings 		-	wg_encode_str
other record 	-	wg_encode_record
```

So let's cover how it works for these three functions, starting with `wg_encode_int`:

```
wg_int wg_encode_int(void* db, wg_int data) {
  gint offset;
#ifdef CHECK
  if (!dbcheck(db)) {
    show_data_error(db,"wrong database pointer given to wg_encode_int");
    return WG_ILLEGAL;
  }
#endif
  if (fits_smallint(data)) {
    return encode_smallint(data);
  } else {
#ifdef USE_DBLOG
    /* Log before allocating. Note this call is skipped when
     * we have a small int.
     */
    if(dbmemsegh(db)->logging.active) {
      if(wg_log_encode(db, WG_INTTYPE, &data, 0, NULL, 0))
        return WG_ILLEGAL;
    }
#endif
    offset=alloc_word(db);
    if (!offset) {
      show_data_error_nr(db,"cannot store an integer in wg_set_int_field: ",data);
#ifdef USE_DBLOG
      if(dbmemsegh(db)->logging.active) {
        wg_log_encval(db, WG_ILLEGAL);
      }
#endif
      return WG_ILLEGAL;
    }
    dbstore(db,offset,data);
#ifdef USE_DBLOG
    if(dbmemsegh(db)->logging.active) {
      if(wg_log_encval(db, encode_fullint_offset(offset)))
        return WG_ILLEGAL; /* journal error */
    }
#endif
    return encode_fullint_offset(offset);
  }
}
```

So we see, it starts off with a check with the `dbcheck` macro (documented elsewhere in these docs, not a huge deal, just checks if the first few bytes of the database are equal to some constant). So assuming we pass that check, there are really two branches it can go down.

The first revolves around storing the integer directly in the record, the second is allocating a block of space, and storing it there. To check if it can directly store the value in the record, it will call the `fits_smallint` macro. It checks if the uppwer and lower 3 bits are not set (equal to `0x00`) via binary shifting:

```
./whitedb.h:#define fits_smallint(i)   ((((i)<<SMALLINTSHFT)>>SMALLINTSHFT)==i)
```

And we see the value of `SMALLINTSHFT` is `0x03`:

```
./whitedb.h:#define SMALLINTSHFT  3
```

Then we see to encode the value, it uses the `encode_smallint` value, effectively just shifts the value up by `SMALLINTSHF`, than ors it with `SMALLINTBITS` (probably to mark it as an integer encoded in this format):

```
./whitedb.h:#define encode_smallint(i) (((i)<<SMALLINTSHFT)|SMALLINTBITS)
```

Which we see the value being `0x03`:

```
./whitedb.h:#define SMALLINTBITS    0x3       ///< int ends with       011
```

So for the other path, we see that it effectively allocated a word worth of space with the `alloc_word` macro, stores the value with `dbstore`, the encodes the offset with the `encode_fullint_offset` Macro:

```
./whitedb.h:#define encode_fullint_offset(i) ((i)|FULLINTBITS)
```

Which effectively just marks the offset via oring it with `FULLINTBITS` which we see is `0x1`:

```
./whitedb.h:#define FULLINTBITS  0x1      ///< full int ptr ends with       01
```

So next up, we have the `wg_encode_str` function, which effectively wraps the `wg_encode_unistr` function:

```
wg_int wg_encode_str(void* db, const char* str, char* lang) {
#ifdef CHECK
  if (!dbcheck(db)) {
    show_data_error(db,"wrong database pointer given to wg_encode_str");
    return WG_ILLEGAL;
  }
  if (str==NULL) {
    show_data_error(db,"NULL string ptr given to wg_encode_str");
    return WG_ILLEGAL;
  }
#endif
  /* Logging handled inside wg_encode_unistr() */
  return wg_encode_unistr(db,str,lang,WG_STRTYPE);
}
```

And for that function:

```
/* ============================================

Universal funs for string, xmlliteral, uri, blob

============================================== */


gint wg_encode_unistr(void* db, const char* str, const char* lang, gint type) {
  gint offset;
  gint len;
#ifdef USETINYSTR
  gint res;
#endif
  char* dptr;
  char* sptr;
  char* dendptr;

  len=(gint)(strlen(str));
#ifdef USE_DBLOG
  /* Log before allocating. */
  if(dbmemsegh(db)->logging.active) {
    gint extlen = 0;
    if(lang) extlen = strlen(lang);
    if(wg_log_encode(db, type, str, len, lang, extlen))
      return WG_ILLEGAL;
  }
#endif
#ifdef USETINYSTR
/* XXX: add tinystr support to logging */
#ifdef USE_DBLOG
#error USE_DBLOG and USETINYSTR are incompatible
#endif
  if (lang==NULL && type==WG_STRTYPE && len<(sizeof(gint)-1)) {
    res=TINYSTRBITS; // first zero the field and set last byte to mask
    if (LITTLEENDIAN) {
      dptr=((char*)(&res))+1; // type bits stored in lowest addressed byte
    } else {
      dptr=((char*)(&res));  // type bits stored in highest addressed byte
    }
    memcpy(dptr,str,len+1);
    return res;
  }
#endif
  if (lang==NULL && type==WG_STRTYPE && len<SHORTSTR_SIZE) {
    // short string, store in a fixlen area
    offset=alloc_shortstr(db);
    if (!offset) {
      show_data_error_str(db,"cannot store a string in wg_encode_unistr",str);
#ifdef USE_DBLOG
      if(dbmemsegh(db)->logging.active) {
        wg_log_encval(db, WG_ILLEGAL);
      }
#endif
      return WG_ILLEGAL;
    }
    // loop over bytes, storing them starting from offset
    dptr = (char *) offsettoptr(db,offset);
    dendptr=dptr+SHORTSTR_SIZE;
    //
    //strcpy(dptr,sptr);
    //memset(dptr+len,0,SHORTSTR_SIZE-len);
    //
    for(sptr=(char *) str; (*dptr=*sptr)!=0; sptr++, dptr++) {}; // copy string
    for(dptr++; dptr<dendptr; dptr++) { *dptr=0; }; // zero the rest
    // store offset to field
#ifdef USE_DBLOG
    if(dbmemsegh(db)->logging.active) {
      if(wg_log_encval(db, encode_shortstr_offset(offset)))
        return WG_ILLEGAL; /* journal error */
    }
#endif
    return encode_shortstr_offset(offset);
    //dbstore(db,ptrtoffset(record)+RECORD_HEADER_GINTS+fieldnr,encode_shortstr_offset(offset));
  } else {
    offset=find_create_longstr(db,str,lang,type,len+1);
    if (!offset) {
      show_data_error_nr(db,"cannot create a string of size ",len);
#ifdef USE_DBLOG
      if(dbmemsegh(db)->logging.active) {
        wg_log_encval(db, WG_ILLEGAL);
      }
#endif
      return WG_ILLEGAL;
    }
#ifdef USE_DBLOG
    if(dbmemsegh(db)->logging.active) {
      if(wg_log_encval(db, encode_longstr_offset(offset)))
        return WG_ILLEGAL; /* journal error */
    }
#endif
    return encode_longstr_offset(offset);
  }
}
```

So for this, similar to int encodings, there are two real code paths. One is if the integer is small enough to be stored in the record itself, it will just encode the string in a small value:

```
  if (lang==NULL && type==WG_STRTYPE && len<(sizeof(gint)-1)) {
    res=TINYSTRBITS; // first zero the field and set last byte to mask
    if (LITTLEENDIAN) {
      dptr=((char*)(&res))+1; // type bits stored in lowest addressed byte
    } else {
      dptr=((char*)(&res));  // type bits stored in highest addressed byte
    }
    memcpy(dptr,str,len+1);
    return res;
  }
```

Now for actually allocating space to store the string in, this is done based on the size of the string, We see that the max size of the shorter strings is `32`:

```
./whitedb.h:#define SHORTSTR_SIZE 32 /** max len of short strings  */
```

We then see that shorter strings are allocated using `alloc_shortstr` (then a for loop to actually write the string), and larger strings are allocated using `find_create_longstr`. Then `encode_shortstr_offset/encode_longstr_offset` are used to encode the offsets, which we see just mark the bits to signify the datatype:

```
./whitedb.h:#define encode_longstr_offset(i) ((i)|LONGSTRBITS)

.	.	.

./whitedb.h:#define encode_shortstr_offset(i) ((i)|SHORTSTRBITS)
```

Then lastly, we have the `wg_encode_record` function for encoding records:

```
// record

wg_int wg_encode_record(void* db, void* data) {
#ifdef CHECK
  if (!dbcheck(db)) {
    show_data_error(db,"wrong database pointer given to wg_encode_char");
    return WG_ILLEGAL;
  }
#endif
#ifdef USE_DBLOG
/* Skip logging values that do not cause storage allocation.
  if(dbh->logging.active) {
    if(wg_log_encode(db, WG_RECORDTYPE, &data, 0, NULL, 0))
      return WG_ILLEGAL;
  }
*/
#endif
  return (wg_int)(encode_datarec_offset(ptrtooffset(db,data)));
}
```

Which we see, basically just returns the actual address of the record:

```
./whitedb.h:#define ptrtooffset(db,realptr) (dbaddr((db),(realptr)))

.	.	.

./whitedb.h:#define offsettoptr(db,offset) ((void*)(dbmemsegbytes(db)+(offset))) /** give real address from offset */
```
