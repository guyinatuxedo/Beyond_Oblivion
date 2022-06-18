# File Loading Process

So now to cover how dragonfly db will actually load a database file

```
error_code RdbLoader::Load(io::Source* src) {
  CHECK(!src_ && src);

  absl::Time start = absl::Now();
  src_ = src;

  IoBuf::Bytes bytes = mem_buf_.AppendBuffer();
  io::Result<size_t> read_sz = src_->ReadAtLeast(bytes, 9);
  if (!read_sz)
    return read_sz.error();

  bytes_read_ = *read_sz;
  if (bytes_read_ < 9) {
    return RdbError(errc::wrong_signature);
  }

  mem_buf_.CommitWrite(bytes_read_);

  {
    auto cb = mem_buf_.InputBuffer();

    if (memcmp(cb.data(), "REDIS", 5) != 0) {
      return RdbError(errc::wrong_signature);
    }

    char buf[64] = {0};
    ::memcpy(buf, cb.data() + 5, 4);

    int rdbver = atoi(buf);
    if (rdbver < 5 || rdbver > RDB_VERSION) {  // We accept starting from 5.
      return RdbError(errc::bad_version);
    }

    mem_buf_.ConsumeInput(9);
  }

  int type;

  /* Key-specific attributes, set by opcodes before the key type. */
  ObjSettings settings;
  settings.now = mstime();
  size_t keys_loaded = 0;

  while (1) {
    /* Read type. */
    SET_OR_RETURN(FetchType(), type);

    /* Handle special types. */
    if (type == RDB_OPCODE_EXPIRETIME) {
      LOG(ERROR) << "opcode RDB_OPCODE_EXPIRETIME not supported";

      return RdbError(errc::invalid_encoding);
    }

    if (type == RDB_OPCODE_EXPIRETIME_MS) {
      int64_t val;
      /* EXPIRETIME_MS: milliseconds precision expire times introduced
       * with RDB v3. Like EXPIRETIME but no with more precision. */
      SET_OR_RETURN(FetchInt<int64_t>(), val);
      settings.SetExpire(val);
      continue; /* Read next opcode. */
    }

    if (type == RDB_OPCODE_FREQ) {
      /* FREQ: LFU frequency. */
      FetchInt<uint8_t>();  // IGNORE
      continue;             /* Read next opcode. */
    }

    if (type == RDB_OPCODE_IDLE) {
      /* IDLE: LRU idle time. */
      uint64_t idle;
      SET_OR_RETURN(LoadLen(nullptr), idle);  // ignore
      (void)idle;
      continue; /* Read next opcode. */
    }

    if (type == RDB_OPCODE_EOF) {
      /* EOF: End of file, exit the main loop. */
      break;
    }

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

    if (type == RDB_OPCODE_RESIZEDB) {
      /* RESIZEDB: Hint about the size of the keys in the currently
       * selected data base, in order to avoid useless rehashing. */
      uint64_t db_size, expires_size;
      SET_OR_RETURN(LoadLen(nullptr), db_size);
      SET_OR_RETURN(LoadLen(nullptr), expires_size);

      ResizeDb(db_size, expires_size);
      continue; /* Read next opcode. */
    }

    if (type == RDB_OPCODE_AUX) {
      RETURN_ON_ERR(HandleAux());
      continue; /* Read type again. */
    }

    if (type == RDB_OPCODE_MODULE_AUX) {
      LOG(ERROR) << "Modules are not supported";
      return RdbError(errc::feature_not_supported);
    }

    if (!rdbIsObjectType(type)) {
      return RdbError(errc::invalid_rdb_type);
    }

    ++keys_loaded;
    RETURN_ON_ERR(LoadKeyValPair(type, &settings));
    settings.Reset();
  }  // main load loop

  /* Verify the checksum if RDB version is >= 5 */
  RETURN_ON_ERR(VerifyChecksum());

  fibers_ext::BlockingCounter bc(shard_set->size());
  for (unsigned i = 0; i < shard_set->size(); ++i) {
    // Flush the remaining items.
    FlushShardAsync(i);

    // Send sentinel callbacks to ensure that all previous messages have been processed.
    shard_set->Add(i, [bc]() mutable { bc.Dec(); });
  }
  bc.Wait();  // wait for sentinels to report.

  absl::Duration dur = absl::Now() - start;
  double seconds = double(absl::ToInt64Milliseconds(dur)) / 1000;
  LOG(INFO) << "Done loading RDB, keys loaded: " << keys_loaded;
  LOG(INFO) << "Loading finished after " << strings::HumanReadableElapsedTime(seconds);

  return kOk;
}
```



```
error_code RdbLoader::HandleAux() {
  /* AUX: generic string-string fields. Use to add state to RDB
   * which is backward compatible. Implementations of RDB loading
   * are required to skip AUX fields they don't understand.
   *
   * An AUX field is composed of two strings: key and value. */
  robj *auxkey, *auxval;

  auto exp = FetchGenericString(RDB_LOAD_NONE);
  if (!exp)
    return exp.error();
  auxkey = (robj*)exp->first;
  exp = FetchGenericString(RDB_LOAD_NONE);
  if (!exp) {
    decrRefCount(auxkey);
    return exp.error();
  }

  auxval = (robj*)exp->first;
  char* auxkey_sds = (sds)auxkey->ptr;
  char* auxval_sds = (sds)auxval->ptr;

  if (auxkey_sds[0] == '%') {
    /* All the fields with a name staring with '%' are considered
     * information fields and are logged at startup with a log
     * level of NOTICE. */
    LOG(INFO) << "RDB '" << auxkey_sds << "': " << auxval_sds;
  } else if (!strcasecmp(auxkey_sds, "repl-stream-db")) {
    // TODO
  } else if (!strcasecmp(auxkey_sds, "repl-id")) {
    // TODO
  } else if (!strcasecmp(auxkey_sds, "repl-offset")) {
    // TODO
  } else if (!strcasecmp(auxkey_sds, "lua")) {
    ServerState* ss = ServerState::tlocal();
    Interpreter& script = ss->GetInterpreter();
    string_view body{auxval_sds, strlen(auxval_sds)};
    string result;
    Interpreter::AddResult add_result = script.AddFunction(body, &result);
    if (add_result == Interpreter::ADD_OK) {
      if (script_mgr_)
        script_mgr_->InsertFunction(result, body);
    } else if (add_result == Interpreter::COMPILE_ERR) {
      LOG(ERROR) << "Error when compiling lua scripts";
    }
  } else if (!strcasecmp(auxkey_sds, "redis-ver")) {
    LOG(INFO) << "Loading RDB produced by version " << auxval_sds;
  } else if (!strcasecmp(auxkey_sds, "ctime")) {
    time_t age = time(NULL) - strtol(auxval_sds, NULL, 10);
    if (age < 0)
      age = 0;
    LOG(INFO) << "RDB age " << strings::HumanReadableElapsedTime(age);
  } else if (!strcasecmp(auxkey_sds, "used-mem")) {
    long long usedmem = strtoll(auxval_sds, NULL, 10);
    LOG(INFO) << "RDB memory usage when created " << strings::HumanReadableNumBytes(usedmem);
  } else if (!strcasecmp(auxkey_sds, "aof-preamble")) {
    long long haspreamble = strtoll(auxval_sds, NULL, 10);
    if (haspreamble)
      LOG(INFO) << "RDB has an AOF tail";
  } else if (!strcasecmp(auxkey_sds, "redis-bits")) {
    /* Just ignored. */
  } else {
    /* We ignore fields we don't understand, as by AUX field
     * contract. */
    LOG(WARNING) << "Unrecognized RDB AUX field: '" << auxkey_sds << "'";
  }

  decrRefCount(auxkey);
  decrRefCount(auxval);

  return kOk;
}
```



```
#0  dfly::RdbLoader::Load (this=0x7fffd0040610, src=0x7fffd00405d0) at ../src/server/rdb_load.cc:218
#1  0x00005555556aa50c in dfly::ServerFamily::LoadRdb (this=0x7fffffffc570, rdb_file=...) at ../src/server/server_family.cc:263
#2  0x000055555568feb7 in dfly::ServerFamily::<lambda()>::operator() (__closure=0x7fffd00407f0) at ../src/server/server_family.cc:250
#3  std::__invoke_impl<void, dfly::ServerFamily::Load(const string&)::<lambda()> > (__f=...) at /usr/include/c++/9/bits/invoke.h:60
#4  std::__invoke<dfly::ServerFamily::Load(const string&)::<lambda()> > (__fn=...) at /usr/include/c++/9/bits/invoke.h:95
#5  std::__apply_impl<dfly::ServerFamily::Load(const string&)::<lambda()>, std::tuple<> > (__t=<synthetic pointer>, __f=...) at /usr/include/c++/9/tuple:1684
#6  std::apply<dfly::ServerFamily::Load(const string&)::<lambda()>, std::tuple<> > (__t=<synthetic pointer>, __f=...) at /usr/include/c++/9/tuple:1694
#7  boost::fibers::worker_context<dfly::ServerFamily::Load(const string&)::<lambda()> >::run_(boost::context::fiber &&) (this=0x7fffd0040a00, c=...) at /usr/include/boost/fiber/context.hpp:436
#8  0x0000555555688473 in std::__invoke_impl<boost::context::fiber, boost::context::fiber (boost::fibers::worker_context<dfly::ServerFamily::Load(const string&)::<lambda()> >::*&)(boost::context::fiber&&), boost::fibers::worker_context<dfly::ServerFamily::Load(const string&)::<lambda()> >*&, boost::context::fiber> (__t=<optimized out>, __f=<optimized out>) at /usr/include/c++/9/bits/invoke.h:73
#9  std::__invoke<boost::context::fiber (boost::fibers::worker_context<dfly::ServerFamily::Load(const string&)::<lambda()> >::*&)(boost::context::fiber&&), boost::fibers::worker_context<dfly::ServerFamily::Load(const string&)::<lambda()> >*&, boost::context::fiber> (__fn=<optimized out>) at /usr/include/c++/9/bits/invoke.h:96
#10 std::_Bind<boost::context::fiber (boost::fibers::worker_context<dfly::ServerFamily::Load(const string&)::<lambda()> >::*(boost::fibers::worker_context<dfly::ServerFamily::Load(const string&)::<lambda()> >*, std::_Placeholder<1>))(boost::context::fiber&&)>::__call<boost::context::fiber, boost::context::fiber&&, 0, 1> (__args=..., this=<optimized out>) at /usr/include/c++/9/functional:402
#11 std::_Bind<boost::context::fiber (boost::fibers::worker_context<dfly::ServerFamily::Load(const string&)::<lambda()> >::*(boost::fibers::worker_context<dfly::ServerFamily::Load(const string&)::<lambda()> >*, std::_Placeholder<1>))(boost::context::fiber&&)>::operator()<boost::context::fiber> (this=<optimized out>) at /usr/include/c++/9/functional:484
#12 std::__invoke_impl<boost::context::fiber, std::_Bind<boost::context::fiber (boost::fibers::worker_context<dfly::ServerFamily::Load(const string&)::<lambda()> >::*(boost::fibers::worker_context<dfly::ServerFamily::Load(const string&)::<lambda()> >*, std::_Placeholder<1>))(boost::context::fiber&&)>&, boost::context::fiber> (__f=...) at /usr/include/c++/9/bits/invoke.h:60
#13 std::__invoke<std::_Bind<boost::context::fiber (boost::fibers::worker_context<dfly::ServerFamily::Load(const string&)::<lambda()> >::*(boost::fibers::worker_context<dfly::ServerFamily::Load(const string&)::<lambda()> >*, std::_Placeholder<1>))(boost::context::fiber&&)>&, boost::context::fiber> (__fn=...) at /usr/include/c++/9/bits/invoke.h:96
#14 std::invoke<std::_Bind<boost::context::fiber (boost::fibers::worker_context<dfly::ServerFamily::Load(const string&)::<lambda()> >::*(boost::fibers::worker_context<dfly::ServerFamily::Load(const string&)::<lambda()> >*, std::_Placeholder<1>))(boost::context::fiber&&)>&, boost::context::fiber> (__fn=...) at /usr/include/c++/9/functional:82
#15 boost::context::detail::fiber_record<boost::context::fiber, boost::context::basic_fixedsize_stack<boost::context::stack_traits>, std::_Bind<boost::context::fiber (boost::fibers::worker_context<dfly::ServerFamily::Load(const string&)::<lambda()> >::*(boost::fibers::worker_context<dfly::ServerFamily::Load(const string&)::<lambda()> >*, std::_Placeholder<1>))(boost::context::fiber&&)> >::run (fctx=<optimized out>, this=<optimized out>) at /usr/include/boost/context/fiber_fcontext.hpp:143
#16 boost::context::detail::fiber_entry<boost::context::detail::fiber_record<boost::context::fiber, boost::context::basic_fixedsize_stack<boost::context::stack_traits>, std::_Bind<boost::context::fiber (boost::fibers::worker_context<dfly::ServerFamily::Load(const string&)::<lambda()> >::*(boost::fibers::worker_context<dfly::ServerFamily::Load(const string&)::<lambda()> >*, std::_Placeholder<1>))(boost::context::fiber&&)> > >(boost::context::detail::transfer_t) (t=...) at /usr/include/boost/context/fiber_fcontext.hpp:80
#17 0x00007ffff77401cf in make_fcontext () from /lib/x86_64-linux-gnu/libboost_context.so.1.71.0
#18 0x0000000000000000 in ?? ()
```


