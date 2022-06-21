
## Database Format

So this is documentation regarding the layout of the database save files, for DragonflyDB. I didn't have time to cover everything.

So the format of the database is variable. The first `0x09` bytes consist of the follwing data:

```
0x00	-	"REDIS" string
0x05	-	Ascii string version (4 bytes of ASCII numbers)
```

Beyond that, the rest of the database is variable. It can consist of the following pieces:

```
RDB_OPCODE_EXPIRETIME
RDB_OPCODE_EXPIRETIME_MS
RDB_OPCODE_FREQ
RDB_OPCODE_IDLE
RDB_OPCODE_EOF
RDB_OPCODE_SELECTDB
RDB_OPCODE_RESIZEDB
RDB_OPCODE_AUX
RDB_OPCODE_MODULE_AUX
KeyValPair
```

With these (from `rdb.h`) being the defining type values:

```
/* Special RDB opcodes (saved/loaded with rdbSaveType/rdbLoadType). */
#define RDB_OPCODE_FUNCTION   246   /* engine data */
#define RDB_OPCODE_MODULE_AUX 247   /* Module auxiliary data. */
#define RDB_OPCODE_IDLE       248   /* LRU idle time. */
#define RDB_OPCODE_FREQ       249   /* LFU frequency. */
#define RDB_OPCODE_AUX        250   /* RDB aux field. */
#define RDB_OPCODE_RESIZEDB   251   /* Hash table resize hint. */
#define RDB_OPCODE_EXPIRETIME_MS 252    /* Expire time in milliseconds. */
#define RDB_OPCODE_EXPIRETIME 253       /* Old expire time in seconds. */
#define RDB_OPCODE_SELECTDB   254   /* DB number of the following keys. */
#define RDB_OPCODE_EOF        255   /* End of the RDB file. */
```

These pieces are loaded in in the while loop of the `RdbLoader::Load` funciton.

## Generic Datatypes

So there are some generic datatypes in this database implementation. This isn't all of them, just a few from what I've seen

#### String

So for a string, it will start off with a size value. Looking at it, it appears that there are `6`, `14`, `32`, and `64` size values. To determine what size it is, it will and the first byte by a mask value `0xc0`, and shift it over to the left by `0x06` (see `LoadLen` for more details). These are the values after the shifting that correlate to which size value:

```
./src/redis/rdb.h:#define RDB_6BITLEN 0
./src/redis/rdb.h:#define RDB_14BITLEN 1
./src/redis/rdb.h:#define RDB_32BITLEN 0x80
./src/redis/rdb.h:#define RDB_64BITLEN 0x81
```

For the cases I've seen, it's just a single byte size value. The size value represents the size of the string in bytes. Proceeding that, it will be the actual string bytes. For example, that the string for `redis-ver`:

```
Hex:
09 72 65 64 69 73 2D 76 65 72

As String
\x09redis-ver
```

#### Long

So for long, it will start off with a single byte `0xC2`, representing the datatype. Proceeding that, will be `4` bytes representing the actual value of whatever it is.

## RDB_OPCODE_AUX

So this is the most common metadata part. This part can signify several different pieces of information, with these being the different parts. This piece is primarily read by the `HandleAux` function in `rdb_load.cc`.

Now next to the topic, will be what `HandleAux` actually does with it. Several of these types actually have nothing implemented for them (listed as `// TODO`). Most of thesewill either do nothing, or print out the value:

```
repl-stream-db		-	Nothing
repl-id				-	Nothing
repl-offset			-	Nothing
lua					-	See below
redis-ver			-	Parsed out the value, prints it
used-mem			-	Parsed out the value, prints it
aof-preamble		-	Parsed out the value, prints it
redis-bits			-	Nothing
```

So, to signal which part it is, the part will begin with a string, of one of those values, signifying which type it is. Proceeding that, there will be a value following it.

For "lua" aux pieces, It will interpet the value as a string. This string will be a lua script code, which will get loaded into memory. In redis, you can save lua scripts which can be called from their hash. This is how it saves it to the database save file.

## RDB_OPCODE_EXPIRETIME_MS

This one will simply take the time associated with the value, and set that as the expiration time:

```
    if (type == RDB_OPCODE_EXPIRETIME_MS) {
      int64_t val;
      /* EXPIRETIME_MS: milliseconds precision expire times introduced
       * with RDB v3. Like EXPIRETIME but no with more precision. */
      SET_OR_RETURN(FetchInt<int64_t>(), val);
      settings.SetExpire(val);
      continue; /* Read next opcode. */
    }
```

## RDB_OPCODE_FREQ

This one will effectively just ignore the value. It will parse out the value, and move onto the next opcode:

```
    if (type == RDB_OPCODE_FREQ) {
      /* FREQ: LFU frequency. */
      FetchInt<uint8_t>();  // IGNORE
      continue;             /* Read next opcode. */
    }
```

## RDB_OPCODE_IDLE

This one will effectively just ignore the value. It will parse out the value, and move onto the next opcode:

```
    if (type == RDB_OPCODE_IDLE) {
      /* IDLE: LRU idle time. */
      uint64_t idle;
      SET_OR_RETURN(LoadLen(nullptr), idle);  // ignore
      (void)idle;
      continue; /* Read next opcode. */
    }
```

## RDB_OPCODE_EOF

So the `EOF` is actually used to resemble the end of the database file. It will break from the greater loop. It is required to have, from what I'm seeing:

```
    if (type == RDB_OPCODE_EOF) {
      /* EOF: End of file, exit the main loop. */
      break;
    }
```

## RDB_OPCODE_SELECTDB

So this appears to deal with some functionality I have not looked at yet. It will start off via parsing out a value that will be a new database ID. It will then check if it exceeds the maximum database limit. Proceeding that, it will set the current database id to the new one.

So, it looks like it's switching the database, not 100% sure exactly what that means yet:

```
    if (type == RDB_OPCODE_SELECTDB) {
      unsigned dbid = 0;

      /* SELECTDB: Select the specified database. */
      SET_OR_RETURN(LoadLen(nullptr), dbid);

      if (dbid > GetFlag(FLAGS_dbnum)) {
        LOG(WARNING) << "database id " << dbid << " exceeds dbnum limit. Try increasing the flag.";

        return RdbError(errc::bad_db_index);
      }

      VLOG(1) << "Select DB: " << dbid;
      for (unsigned i = 0; i < shard_set->size(); ++i) {
        // we should flush pending items before switching dbid.
        FlushShardAsync(i);

        // Active database if not existed before.
        shard_set->Add(i, [dbid] { EngineShard::tlocal()->db_slice().ActivateDb(dbid); });
      }

      cur_db_index_ = dbid;
      continue; /* Read next opcode. */
    }
```


## RDB_OPCODE_RESIZEDB

So looking at this opcode, it appears to parse out two valeus from the file, then call the `ResizeDb` function:

```
    if (type == RDB_OPCODE_RESIZEDB) {
      /* RESIZEDB: Hint about the size of the keys in the currently
       * selected data base, in order to avoid useless rehashing. */
      uint64_t db_size, expires_size;
      SET_OR_RETURN(LoadLen(nullptr), db_size);
      SET_OR_RETURN(LoadLen(nullptr), expires_size);

      ResizeDb(db_size, expires_size);
      continue; /* Read next opcode. */
    }
```

Which just wraps the `DCHECK_LT` macro. Looking through the code, it appears to just be checking that those values are less than `1 << 31` = `0x80000000`, yet not actuallt do anything with them after that.

```
void RdbLoader::ResizeDb(size_t key_num, size_t expire_num) {
  DCHECK_LT(key_num, 1U << 31);
  DCHECK_LT(expire_num, 1U << 31);
}
```

## RDB_OPCODE_MODULE_AUX

This one just outputs an error, does not appear to be supported yet:

```
    if (type == RDB_OPCODE_MODULE_AUX) {
      LOG(ERROR) << "Modules are not supported";
      return RdbError(errc::feature_not_supported);
    }
```

## Not Valid Type

If it gets passed of all of those values, it will check if it is an rdb object type, with the `rdbIsObjectType` function.

```
    if (!rdbIsObjectType(type)) {
      return RdbError(errc::invalid_rdb_type);
    }
```

The `rdbIsObjectType` function will effectively just check if the type value is between either `0-7` or `9-18`, so those are the valid types for actual records:

```
./src/redis/rdb.h:#define rdbIsObjectType(t) ((t >= 0 && t <= 7) || (t >= 9 && t <= 18))
```

## Key Value Pair

So past all of that, you have actual records that are loaded with the `LoadKeyValPair` function:

```
    ++keys_loaded;
    RETURN_ON_ERR(LoadKeyValPair(type, &settings));
    settings.Reset();
```

Now looking further, we see that these are all of the defined record types:

```
/* Map object types to RDB object types. Macros starting with OBJ_ are for
 * memory storage and may change. Instead RDB types must be fixed because
 * we store them on disk. */
#define RDB_TYPE_STRING 0
#define RDB_TYPE_LIST   1
#define RDB_TYPE_SET    2
#define RDB_TYPE_ZSET   3
#define RDB_TYPE_HASH   4
#define RDB_TYPE_ZSET_2 5 /* ZSET version 2 with doubles stored in binary. */
#define RDB_TYPE_MODULE 6
#define RDB_TYPE_MODULE_2 7 /* Module value with annotations for parsing without
                               the generating module being loaded. */
/* NOTE: WHEN ADDING NEW RDB TYPE, UPDATE rdbIsObjectType() BELOW */

/* Object types for encoded objects. */
#define RDB_TYPE_HASH_ZIPMAP    9
#define RDB_TYPE_LIST_ZIPLIST  10
#define RDB_TYPE_SET_INTSET    11
#define RDB_TYPE_ZSET_ZIPLIST  12
#define RDB_TYPE_HASH_ZIPLIST  13
#define RDB_TYPE_LIST_QUICKLIST 14
#define RDB_TYPE_STREAM_LISTPACKS 15
#define RDB_TYPE_HASH_LISTPACK 16
#define RDB_TYPE_ZSET_LISTPACK 17
#define RDB_TYPE_LIST_QUICKLIST_2   18
```

Now looking at the `LoadKeyValPair` function, we see that it effectively uses the `ReadObj` function to actually read the object:

```
  SET_OR_RETURN(ReadKey(), key);

  auto key_cleanup = absl::MakeCleanup([key] { sdsfree(key); });

  SET_OR_RETURN(ReadObj(type), val);
```

Which we see right here:

```
io::Result<robj*> RdbLoader::ReadObj(int rdbtype) {
  io::Result<robj*> res_obj = nullptr;
  io::Result<OpaqueBuf> fetch_res;

  switch (rdbtype) {
    case RDB_TYPE_STRING:
      /* Read string value */
      fetch_res = FetchGenericString(RDB_LOAD_NONE);
      if (!fetch_res)
        return fetch_res.get_unexpected();
      res_obj = (robj*)fetch_res->first;
      break;
    case RDB_TYPE_SET:
      res_obj = ReadSet();
      break;
    case RDB_TYPE_SET_INTSET:
      res_obj = ReadIntSet();
      break;
    case RDB_TYPE_HASH_ZIPLIST:
      res_obj = ReadHZiplist();
      break;
    case RDB_TYPE_HASH:
      res_obj = ReadHSet();
      break;
    case RDB_TYPE_ZSET:
    case RDB_TYPE_ZSET_2:
      res_obj = ReadZSet(rdbtype);
      break;
    case RDB_TYPE_ZSET_ZIPLIST:
      res_obj = ReadZSetZL();
      break;
    case RDB_TYPE_LIST_QUICKLIST:
      res_obj = ReadListQuicklist(rdbtype);
      break;
    default:
      LOG(ERROR) << "Unsupported rdb type " << rdbtype;
      return Unexpected(errc::invalid_encoding);
  }

  return res_obj;
}
```

So interestingly enough, even though there are like 10-20 defined record types, roughly half of them are actually implemented here. Which we see, each type has a corresponding function to read that type:

```
RDB_TYPE_STRING 			:	FetchGenericString
RDB_TYPE_SET 				:	ReadSet
RDB_TYPE_SET_INTSET 		:	ReadIntSet
RDB_TYPE_HASH_ZIPLIST 		:	ReadHZiplist
RDB_TYPE_HASH 				:	ReadHSet
RDB_TYPE_ZSET 				:	ReadZSet
RDB_TYPE_ZSET_2 			:	ReadZSet
RDB_TYPE_ZSET_ZIPLIST 		:	ReadZSetZL
RDB_TYPE_LIST_QUICKLIST 	:	ReadListQuicklist
```

If a defined type (a list this instance) is used that isn't implemented, this error message is displayed:

```
E20220619 11:36:35.545209  8614 rdb_load.cc:838] Unsupported rdb type 1
```

#### RDB_TYPE_STRING

So this is probably the most simple type of record. There is a key, and a value. They are both strings. So the record will start off with a `0x00` to signify the type. Then there will be the size of the key, followed by the key string. Then there will be the size of the value, followed by the value string. Here is an example, of a record where the key is `y`, and the value is `yyyyy`:

```
00 01 79 05 79 79 79 79 79
```

#### RDB_TYPE_HASH_ZIPLIST


So a hash ziplist, is sort of like an array of strings. Here is roughly the format:

```
Data Type Value 			:	RDB_TYPE_HASH_ZIPLIST single byte 0x0D (13)
Ziplist Name				:	
Total Ziplist Size 			:	
Data Segment Size 			:	
Number of Key/Value Pairs 	:	integer representing the 

Key/Value pairs				:	The key/value pairs, see next thing for format

End of Ziplist 				:	0xff (or in some instances 0xffff), means end of ziplist
```

The key/value pairs have this format:

```
prev size  			:	Size of the previous element
current size 		:	Size of the current element
key value 			:	The value of the key
prev size 			:	Size of the previous element
current size 		:	Size of the current element
value value 		:	Value of the value
```

For an example of the key/value pairs, here are two key/value pairs (key0/val0 and key11/value11111). `key0` is the first element, so it's prev size is `0x00`:

```
00 04 6b 65 79 30 06 04 76 61 6c 30 06 05 6b 65 79 31 31 07 08 76 61 6c 31 31 31 31 31
```

Here is an example of a total ziplist:





So here are some checks that are done when the ziplist is being loaded, which are done in the `ziplistValidateIntegrity` function.

0.) End of the header size actually points to EOF:

```
    /* the last byte must be the terminator. */
    if (zl[size - ZIPLIST_END_SIZE] != ZIP_END)
        return 0;
```

1.) Ziplist length isn't extending beyond end of hashset:

```
    /* make sure the tail offset isn't reaching outside the allocation. */
    if (intrev32ifbe(ZIPLIST_TAIL_OFFSET(zl)) > size - ZIPLIST_END_SIZE)
        return 0;
```

Here it will iterate through all of the entries of the ziplist. It will check several things. First off, it will check that the current length does not extend past the end of the record. It will also check that the previous size matches the actual size. It will also keep track of the number of key/value pairs it counted.

```
    while(*p != ZIP_END) {
        struct zlentry e;
        /* Decode the entry headers and fail if invalid or reaches outside the allocation */
        if (!zipEntrySafe(zl, size, p, &e, 1))
            return 0;

        /* Make sure the record stating the prev entry size is correct. */
        if (e.prevrawlen != prev_raw_size)
            return 0;

        /* Optionally let the caller validate the entry too. */
        if (entry_cb && !entry_cb(p, header_count, cb_userdata))
            return 0;

        /* Move to the next entry */
        prev_raw_size = e.headersize + e.len;
        prev = p;
        p += e.headersize + e.len;
        count++;
    }
```

Next up, it will actually check that the end of the ziplist matches the expected end:

```
    /* Make sure 'p' really does point to the end of the ziplist. */
    if (p != zl + bytes - ZIPLIST_END_SIZE)
        return 0;

    /* Make sure the <zltail> entry really do point to the start of the last entry. */
    if (prev != NULL && prev != ZIPLIST_ENTRY_TAIL(zl))
        return 0;
```

It will also check that the number of key/value pairs it counted matched 

```
    /* Check that the count in the header is correct */
    if (header_count != UINT16_MAX && count != header_count)
        return 0;
```
