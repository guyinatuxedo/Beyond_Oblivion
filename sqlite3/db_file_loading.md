# DB Loading

So a database is loaded from a file, into memory. This document analyzes that process

So the database is not really loaded, until you run your first query. When you run the first query, a series of function calls will be made to initialize it. The file is read with a `fread` call at this spot:

```
gef➤  bt
#0  __GI__IO_fread (buf=0x7fffffffb6d0, size=0x10, count=0x1, fp=0x555555692700) at iofread.c:31
#1  0x000055555557c8a3 in fread (__stream=0x555555692700, __n=0x1, __size=0x10, __ptr=0x7fffffffb6d0) at /usr/include/x86_64-linux-gnu/bits/stdio2.h:297
#2  deduceDatabaseType (zName=0x5555556938d0 "x", dfltZip=0x0) at shell.c:14424
#3  0x000055555557cb12 in open_db (p=0x7fffffffcd80, openFlags=0x1) at shell.c:14713
#4  0x00005555555859bd in open_db (openFlags=0x1, p=0x7fffffffcd80) at shell.c:18851
#5  do_meta_command (zLine=<optimized out>, p=<optimized out>) at shell.c:18883
#6  0x0000555555587fbd in process_input (p=0x7fffffffcd80) at shell.c:20670
#7  0x0000555555564c9a in main (argc=<optimized out>, argv=0x7fffffffe0a8) at shell.c:21504
```

We will be looking at the `sqlite3InitOne` function. This is the function which is responsible for initializing one database file.

So the first that it does, is it creates a cursor to the database, and starts a transaction on the database using `sqlite3BtreeBeginTrans`. Proceeding that, it will read the database meta information, with this loop:

```
  for(i=0; i<ArraySize(meta); i++){
    sqlite3BtreeGetMeta(pDb->pBt, i+1, (u32 *)&meta[i]);
  }
```

The metadata consists of the portion of the file header portion with `4` byte values starting at `0x28` (`40`). For all of the metadata values except for the data version, it just calculates the offset to the data via `36 + idx*4`, and grabs the bytes using `get4byte`. For the data version, it uses `sqlite3PagerDataVersion` which returns the data version from the Btree Pager struct, within the Btree struct. Proceeding that, it will set the text encoding from the metedata value. Proceeding that it will also set the default cache size from the metadata value. After that it will set the Btree file format from the metadata value.

Proceeding that, it will read the schema of the database. Now it will do this via a select query that actually gets ran like any other sql query. The sql query I've seen used is `SELECT*FROM"main".sqlite_master ORDER BY rowid`. For a database with a table, and an index for that table, these are the opcodes which will execute:

```
0x3e:	OP_Init
0x02:	OP_Transaction
0x0b:	OP_Goto
0x61:	OP_OpenRead
0x25:	OP_Rewind
0x5a:	OP_Column
0x5a:	OP_Column
0x5a:	OP_Column
0x5a:	OP_Column
0x5a:	OP_Column
0x51:	OP_ResultRow
0x05:	OP_Next
0x5a:	OP_Column
0x5a:	OP_Column
0x5a:	OP_Column
0x5a:	OP_Column
0x5a:	OP_Column
0x51:	OP_ResultRow
0x05:	OP_Next
0x44:	OP_Halt
```

So, for each row that it gets, it represents one table/index/view/trigger that the database has. For each select, there are five columns. The first resembles what it is (like a table/index/etc). The second is the name of the object, and the third is the name of the database. The fourth is the number for the root page. The fifth is the SQL text to create it. Now the function that runs sql queries is `sqlite3_exec`. This takes a callback function, which in that instance is `sqlite3InitCallback` which sets the schema. The third argument to `sqlite3InitCallback` is a char char ptr called `argv`, which holds the five values from the select statement. Here are some examples:

Example 0:

```
──────────────────────────────────────────────────────────────────────────────────────── code:x86:64 ────
   0x55555562cea7 <sqlite3_prepare16_v3+7> or     cl, 0x80
   0x55555562ceaa <sqlite3_prepare16_v3+10> jmp    0x55555562cbc0 <sqlite3Prepare16>
   0x55555562ceaf                  nop    
 → 0x55555562ceb0 <sqlite3InitCallback+0> endbr64 
   0x55555562ceb4 <sqlite3InitCallback+4> push   r13
   0x55555562ceb6 <sqlite3InitCallback+6> push   r12
   0x55555562ceb8 <sqlite3InitCallback+8> mov    r12, rdx
   0x55555562cebb <sqlite3InitCallback+11> push   rbp
   0x55555562cebc <sqlite3InitCallback+12> mov    rbp, rdi
──────────────────────────────────────────────────────────────────────────── source:sqlite3.c+130171 ────

.	.	.

gef➤  x/6g $rdx
0x5555556a2778:	0x00005555556a1850	0x00005555556a25d0
0x5555556a2788:	0x00005555556a2650	0x00005555556a26d0
0x5555556a2798:	0x00005555556a27d0	0x0000000000000000
gef➤  x/s 0x00005555556a1850
0x5555556a1850:	"table"
gef➤  x/s 0x00005555556a25d0
0x5555556a25d0:	"x"
gef➤  x/s 0x00005555556a2650
0x5555556a2650:	"x"
gef➤  x/s 0x00005555556a26d0
0x5555556a26d0:	"2"
gef➤  x/s 0x00005555556a27d0
0x5555556a27d0:	"CREATE TABLE x (y int, z varchar)"

```

Example 1:

```
gef➤  x/6g $rdx
0x5555556a2778:	0x00005555556a1850	0x00005555556a25d0
0x5555556a2788:	0x00005555556a2650	0x00005555556a26d0
0x5555556a2798:	0x00005555556a27d0	0x0000000000000000
gef➤  x/s 0x00005555556a1850
0x5555556a1850:	"index"
gef➤  x/s 0x00005555556a25d0
0x5555556a25d0:	"index_x"
gef➤  x/s 0x00005555556a2650
0x5555556a2650:	"x"
```

Example 2:

```
gef➤  x/6g $rdx
0x7fffffffb8c0:	0x000055555565da2e	0x000055555564e7e2
0x7fffffffb8d0:	0x000055555564e7e2	0x000055555564eae0
0x7fffffffb8e0:	0x0000555555659c50	0x0000000000000000
gef➤  x/s 0x000055555565da2e
0x55555565da2e:	"table"
gef➤  x/s 0x000055555564e7e2
0x55555564e7e2:	"sqlite_master"
gef➤  x/s 0x000055555564e7e2
0x55555564e7e2:	"sqlite_master"
gef➤  x/s 0x000055555564eae0
0x55555564eae0:	"1"
gef➤  x/s 0x0000555555659c50
0x555555659c50:	"CREATE TABLE x(type text,name text,tbl_name text,rootpage int,sql text)"
```

Now the `sqlite3InitCallback` function will run the sql code for creating the object. It will do so without generating more vdbe opcodes. It will do so with the `sqlite3_finalize` and `sqlite3Prepare` functions.
