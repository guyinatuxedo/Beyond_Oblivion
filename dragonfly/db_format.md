
## Database Format

So this is documentation regarding the layout of the database save files, for DragonflyDB.

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

## Generic Datatypes

So there are some generic datatypes in this database implementation.

#### String

So strings from what I've seen are typicall read with the ``

## RDB_OPCODE_AUX

So this is the most common metadata part. This part can signify several different pieces of information, with these being the different parts. This piece is primarily read by the `HandleAux` function in `rdb_load.cc`:

```
repl-stream-db		-	Nothing
repl-id				-	Nothing
repl-offset			-	Nothing
lua					-	
redis-ver			-	
used-mem			-	
aof-preamble		-	
redis-bits			-	
```

So, to signal which part it is, the part will begin with a string, of one of those values, signifying which type it is. Proceeding that, there will be a value following it.






