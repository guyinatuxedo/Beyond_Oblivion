

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

/* Module serialized values sub opcodes */
#define RDB_MODULE_OPCODE_EOF   0   /* End of module value. */
#define RDB_MODULE_OPCODE_SINT  1   /* Signed integer. */
#define RDB_MODULE_OPCODE_UINT  2   /* Unsigned integer. */
#define RDB_MODULE_OPCODE_FLOAT 3   /* Float. */
#define RDB_MODULE_OPCODE_DOUBLE 4  /* Double. */
#define RDB_MODULE_OPCODE_STRING 5  /* String. */
```




```
--alsologtostderr
```


```
RdbLoader::Load ()	- 
RdbLoader::HandleAux () - 
RdbLoader::LoadKeyValPair () -
```

```
auto RdbLoader::ReadKey() -> io::Result<sds> {
  auto res = FetchGenericString(RDB_LOAD_SDS);
  if (res) {
    sds k = (sds)res->first;
    DVLOG(2) << "Read " << std::string_view(k, sdslen(k));
    return k;
  }
  return res.get_unexpected();
}
```

```
#define RDB_TYPE_STRING 0
#define RDB_TYPE_LIST   1
#define RDB_TYPE_SET    2
#define RDB_TYPE_ZSET   3
#define RDB_TYPE_HASH   4
#define RDB_TYPE_ZSET_2 5 /* ZSET version 2 with doubles stored in binary. */
#define RDB_TYPE_MODULE 6
#define RDB_TYPE_MODULE_2 7 /* Module value with annotations for parsing without
                               the generating module being loaded. */
```


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





```
auto RdbLoader::FetchGenericString(int flags) -> io::Result<OpaqueBuf> {
  bool isencoded;
  size_t len;

  SET_OR_UNEXPECT(LoadLen(&isencoded), len);

  if (isencoded) {
    switch (len) {
      case RDB_ENC_INT8:
      case RDB_ENC_INT16:
      case RDB_ENC_INT32:
        return FetchIntegerObject(len, flags, NULL);
      case RDB_ENC_LZF:
        return FetchLzfStringObject(flags);
      default:
        LOG(ERROR) << "Unknown RDB string encoding len " << len;
        return Unexpected(errc::rdb_file_corrupted);
    }
  }

  bool encode = (flags & RDB_LOAD_ENC) != 0;
  bool plain = (flags & RDB_LOAD_PLAIN) != 0;
  bool sds = (flags & RDB_LOAD_SDS) != 0;

  if (plain || sds) {
    if (plain && len == 0) {
      return make_pair(nullptr, 0);
    }

    char* buf = plain ? (char*)zmalloc(len) : sdsnewlen(SDS_NOINIT, len);
    error_code ec = FetchBuf(len, buf);
    if (ec) {
      if (plain)
        zfree(buf);
      else
        sdsfree(buf);
      return make_unexpected(ec);
    }

    return make_pair(buf, len);
  }

  robj* o = encode ? createStringObject(SDS_NOINIT, len) : createRawStringObject(SDS_NOINIT, len);
  error_code ec = FetchBuf(len, o->ptr);
  if (ec) {
    decrRefCount(o);
    return make_unexpected(ec);
  }
  return make_pair(o, len);
}
```



