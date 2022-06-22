# Dragonfly DB

So this is some documentation regarding the internal working of dragonfly db (`https://github.com/dragonflydb/dragonfly`).

## Usage

Here is just a little docs describing how to use dragonfly db.

To run the database listening on port `6405`:

```
$	./dragonfly --alsologtostderr --port 6405
I20220621 18:45:02.028090  3031 init.cc:56] ./dragonfly running in opt mode.
I20220621 18:45:02.028959  3031 dfly_main.cc:179] maxmemory has not been specified. Deciding myself....
I20220621 18:45:02.029004  3031 dfly_main.cc:184] Found 17.12GiB available memory. Setting maxmemory to 13.70GiB
I20220621 18:45:02.029546  3032 proactor.cc:456] IORing with 1024 entries, allocated 102720 bytes, cq_entries is 2048
I20220621 18:45:02.031459  3031 proactor_pool.cc:66] Running 8 io threads
I20220621 18:45:02.032938  3031 server_family.cc:190] Data directory is "/Hackery/databases/dragonfly/build-opt"
I20220621 18:45:02.033068  3031 server_family.cc:224] Loading /Hackery/databases/dragonfly/build-opt/dump-2022-06-20T17:22:35.rdb
I20220621 18:45:02.033366  3039 rdb_load.cc:551] Loading RDB produced by version 999.999.999
I20220621 18:45:02.033406  3034 listener_interface.cc:79] sock[29] AcceptServer - listening on port 6405
I20220621 18:45:02.033432  3039 rdb_load.cc:556] RDB age 1.35 days
I20220621 18:45:02.033471  3039 rdb_load.cc:559] RDB memory usage when created 1.18MiB
I20220621 18:45:02.034076  3039 rdb_load.cc:370] Done loading RDB, keys loaded: 1
I20220621 18:45:02.034111  3039 rdb_load.cc:371] Loading finished after 0 us
```

To connect to the database client to the server on port `6405`:

```
$	redis-cli -p 6405
127.0.0.1:6405> 
```

To set a value, and then retrieve that value:

```
127.0.0.1:6405> set testkey testval
OK
127.0.0.1:6405> get testkey
"testval"
```

To save the database file:

```
127.0.0.1:6405> save
OK
```

Which is saved to this file, which we see in the server log:

```
I20220621 18:48:16.983289  3032 server_family.cc:440] Saving "dump-2022-06-22T01:48:16.rdb" finished after 1 ms
```
