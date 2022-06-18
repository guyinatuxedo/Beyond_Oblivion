
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






