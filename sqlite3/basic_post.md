# Sqlite3 Basic Functionality

So this is a basic high-level look at some of the internal functionality of how sqlite3 performs simple operations.

A few things right off the bat. The database is stored in a file (named the same thing as a database). The database is loaded into memory (scanned into memory with `pread64` within `unixRead`), operations are performed in memory to lookup/create/edit/delete records, then written back to the file. One thing to note, the contents of the file for the tables, do match the data in memory.

Also another thing, sqlite3 has a VM (Virtual-Machine) implementation. When you pass it a query, it will lex it, tokenize it, and inevitably transform it into a series of opcodes, which will be executed. Also there are two types of inputs, sql queries and meta commands. Meta commands deal with the configuration/settings of sqlite3 (like what db we are currently using), and sql queries are just sql queries. Meta commands do not go through this VM opcode process.

## Query Walkthroughs

So this is a section, which will look at different types of basic queries on tables, and what operations happen to actually accomplish that.

#### Select Statement

So for this, we are going to have this statement:
```
sqlite> select * from y;
hope remains|5
hearts of|6
```

Which executes these ops:

Op Codes Executed:
```
0x3e: OP_Init
0x02: OP_Transaction
0x0b: OP_Goto
0x61: OP_OpenRead
0x25: OP_Rewind
0x5a: OP_Column
0x5a: OP_Column
0x51: OP_ResultRow
0x05: OP_Next
0x5a: OP_Column
0x5a: OP_Column
0x51: OP_ResultRow
0x05: OP_Next
0x44: OP_Halt
```

So we can see here, the operation starts off with initializing an operation. Then it opens up the table using `OP_OpenRead`, and uses `OP_Rewind` to have the cursor pointed at the first row of the table. It then iterates through each column grabbing the value with `OP_Column`, accumulating the results with `OP_ResultRow`, and moving forward to the next row with `OP_Next`. After it iterates through all of the rows, it calls `OP_HALT`.

For this statement with a `where` clause:
```
sqlite> select * from y where z = 111;
111|111
```

With this db contents:
```
sqlite> select * from y;
15935728|20
15935728|20
111|111
```

Has these ops executed:
```
0x3e: OP_Init
0x02: OP_Transaction
0x45: OP_Integer
0x0b: OP_Goto
0x61: OP_OpenRead
0x25: OP_Rewind
0x5a: OP_Column
0x34: OP_Ne
0x05: OP_Next
0x5a: OP_Column
0x34: OP_Ne
0x05: OP_Next
0x5a: OP_Column
0x34: OP_Ne
0x5a: OP_Column
0x5a: OP_Column
0x51: OP_ResultRow
0x05: OP_Next
0x44: OP_Halt
```

So for this operation, we see that in both instances it will iterate through all of the records in the table. On a high level, it does that by going to the first record, and iterating through the rest of them to the last, 1 at a time. It does this through the use of the `OP_Rewind` (to set the cursor to the first entry, and configure it in a forward motion), and `OP_Next` (to move it to the next).

Now when it looks at a record, it does so by looking at individual columns. To look at the value of a column, it does this via the `OP_Column` operation. In this instance where there was a `where` conditional, it will do a comparison against the column with the conditional utilizing a `OP_Ne` operation. Now when it gets to a record that it actually wants to return from the select statement, it will first grab all of the columns from the record utilizing the `OP_Column` opcode. Then it will aggregate them together into a form that can be returned from it using the `OP_ResultRow` opcode. In both cases, when a `OP_Next` instruction moves the cursor past the end of the table, it exits with a `OP_Halt` instruction.

Now before we dive deeper into the internals of what is happening. Now if you look at the source code for `sqlite3`, it is very clear that they intend to use B-Trees in order to get performance increases. However in this case, it doesn't appear to use that functionality. I believe the reason why for that is because we don't use an Index, which is effectively where you can specify certain behavior which will lead to performance improvements. Now this is a fairly simple table, with two columns that are exposed to the user (the rowid is realistically hidden), so realistically if we are to query this table looking for records, we have to look at every record in a linear fashion.

Now to take a slightly deeper dive into what is happening. It uses a BtCursor to iterate through the Btree, with the Btree representing a table. The Btree consists of the data structure itself (which has sub-data structures within it), and the Btree cursor will effectively just point to a particular spot in the Btree. Now within the `BtCursor` data structure, there is a value called `ix`. This value from what I've seen is effectively the index to the record the cursor is pointing to. The `OP_Rewind` opcode will set this value to `0x00`, abd the `OP_Next` opcode will increment this value by `0x01`.

Now for how it actually extracts column values from a record (which is done with the `OP_Column` opcode). To get it's location in memory, it will take the base address of the record. It then has a list of offsets to particular columns, which it will add that columns offset to the base address to get the address for that column, in that row. It will usually use the `sqlite3VdbeSerialGet` function to extract the column data from what I've seen.

#### Insert Statement

So for this, we are going to have this statement:
```
insert into y (x, z) values ("15935728", 10);
```

Op codes Executed
```
0x3e: OP_Init
0x02: OP_Transaction
0x0b: OP_Goto
0x62: OP_OpenWrite
0x74: OP_String8
0x45: OP_Integer
0x7a: OP_NewRowid
0x5c: OP_MakeRecord
0x7b: OP_Insert
0x44: OP_Halt
```
So we can see here, the operation starts off with the standard initialization. Proceeding that, it opens the table using `OP_OpenWrite`. After that, it will prepare the data values. For a string it will use `OP_String8`, and for the integer it will use `OP_Integer`. It also generates a new id for the row using the `OP_NewRowid` opcode. It then constructs the record from the prepared values using `OP_MakeRecord`, and inserts it into the table using `OP_Insert`, and then halts the operation.

Now it creates the record with the `OP_MakeRecord` opcode. This will construct the record by writing individual column values using the `sqlite3VdbeSerialPut` function (although in the binary, it appears that an optimization replaced this function call with something else). Then it inserts the record into the tree with the use of the `sqlite3BtreeInsert` function, which relies on the `insertCell` function. It will increment by one the `nCell` for the memory page (struct `MemPage->nCell`) the cell is stored in to signify the new cell.

#### Update Statement

So for this, we are going to have this statement (same table content as select statement):
```
update y set z = 0;
```

Op codes executed:
```
0x3e: OP_Init
0x02: OP_Transaction
0x0b: OP_Goto
0x48: OP_Null
0x62: OP_OpenWrite
0x25: OP_Rewind
0x82: OP_Rowid
0x32: OP_IsNull
0x5a: OP_Column
0x45: OP_Integer
0x5c: OP_MakeRecord
0x7b: OP_Insert
0x05: OP_Next
0x82: OP_Rowid
0x32: OP_IsNull
0x5a: OP_Column
0x45: OP_Integer
0x5c: OP_MakeRecord
0x7b: OP_Insert
0x05: OP_Next
0x44: OP_Halt
```

So for this example, there are two records in the table at the time of the update, which both of them get updated. We can see what it is effectively doing, is just creating new records with the updated values, and inserting them at the spots the old records were at. It grabs the ids using the `OP_Rowid` opcode.

So effectively, what is happening with update, is it's just inserting new records with the ids of the records it is replacing. One thing I've seen, if it can fit, the new record will occupy the same space as the old record.

#### Delete Statement

So for this, we are going to have this statement (same table content as select statement):
```
delete from y;
```

```
0x3e: OP_Init
0x02: OP_Transaction
0x0b: OP_Goto
0x8c: OP_Clear
0x44: OP_Halt
```

So this operation effectively just runs the `OP_Clear` opcode, which clears all of the records from a table without deleting the table. This perfectly matches the operation, since the operation itself is supposed to delete all of the records for the table, but leave the table.

#### Create Table Statement

So for this, we are going to have this statement (same table content as select statement):
```
create table y (x varchar(100), z int);
```

Which executes these ops:
```
0x3e: OP_Init
0x02: OP_Transaction
0x0b: OP_Goto
0x5e: OP_ReadCookie
0x12: OP_If
0x8e: OP_CreateBtree
0x62: OP_OpenWrite
0x7a: OP_NewRowid
0x4a: OP_Blob
0x7b: OP_Insert
0x75: OP_Close
0x75: OP_Close
0x48: OP_Null
0x62: OP_OpenWrite
0x1f: OP_SeekRowid
0x82: OP_Rowid
0x32: OP_IsNull
0x74: OP_String8
0x74: OP_String8
0x74: OP_String8
0x4e: OP_Int64
0x74: OP_String8
0x5c: OP_MakeRecord
0x7b: OP_Insert
0x5f: OP_SetCookie
0x90: OP_ParseSchema (recursive VM)
    0x3e: OP_Init
    0x02: OP_Transaction
    0x74: OP_String8
    0x74: OP_String8
    0x0b: OP_Goto
    0x61: OP_OpenRead
    0x25: OP_Rewind
    0x5a: OP_Column
    0x34: OP_Ne
    0x5a: OP_Column
    0x35: OP_Eq
    0x5a: OP_Column
    0x5a: OP_Column
    0x5a: OP_Column
    0x5a: OP_Column
    0x5a: OP_Column
    0x51: OP_ResultRow
    0x05: OP_Next
    0x44: OP_Halt
0x44: OP_Halt
```

So here, we have the opcodes which will create a new table. A new tree is allocated with the `btreeCreateTable` function, which is referenced with the `OP_CreateBtree` opcode. One thing to note, the `OP_ParseSchema` will make a recursive vm to run within this one.

#### Drop Table Statement

```
drop table y;
```

Which executes these ops:

OpCodes Executed:
```
0x3e: OP_Init
0x02: OP_Transaction
0x74: OP_String8
0x74: OP_String8
0x0b: OP_Goto
0x48: OP_NULL
0x62: OP_OpenWrite
0x25: OP_Rewind
0x5a: OP_Column
0x34: OP_Ne
0x5a: OP_Column
0x35: OP_Eq
0x82: OP_Rowid
0x7d: OP_Delete
0x05: OP_Next
0x8b: OP_Destroy
0x48: OP_NULL
0x65: OP_OpenEphemeral
0x14: OP_IfNot
0x62: OP_OpenWrite
0x25: OP_Rewind
0x92: OP_DropTable
0x5f: OP_SetCookie
0x44: OP_Halt
```

So these are the opcodes which will drop a table. The opcode `OP_DropTable` will call the `deleteTable` function (wrapped behind a few function calls deep). When it drops it, it will effectively just go through, and free the memory associated with the table.

## Operations

The purpose of this section is to briefly explain some of the operation of common opcodes within the vm. One thing to note, the registers which are used as arguments to the opcodes (like `rdi/esi/ax`) are named `P1/P2/P3/.../P-N`. The primary function in which these opcodes happen in, is `sqlite3VdbeExec` in `vdbe.c`, in a massive switch statement.

#### OP_Blob

This opcode will transfer the blob stored in `P4` (with a ptr), into the `P2` register, that is `P1` bytes long.

#### OP_Close

This opcode will close the cursor, which is specified by `P1`.

#### OP_CreateBtree

The purpose of this opcode is to create a new Btree in the main database file, which models the new table. This is done through the use of the `sqlite3BtreeCreateTable` function.

#### OP_Column

This opcode is responsible for getting the value of a particular column, from a particular row.

#### OP_Destroy

This opcode will delete a table from a database, whose root page is not in the database file specified by `P1`.

#### OP_DropTable

This opcode will drop a table from a db. It will do this via removing the data structures that describe the table (specified in `p4`), from the database specified in `p1`. It does this through the use of the `sqlite3UnlinkAndDeleteTable` function.

#### OP_Eq

This opcode runs a conditional. If the conditional is true, then `p2` is jumped to. The two values being compared are `p3` and `p1`. The conditional here is equal.

#### OP_Goto

This opcode will jump to the address stored in `P2`.

#### OP_Halt

This opcode will halt execution.

#### OP_If

This opcode is a conditional. If the value of `p1` is numerical and non-zero, the condition is considered true. If the value of `p1` is null, and if the value of `p3` is numerical and non-zero, the conditional is considered true. If the conditional is true, than it jumps to `p2`.

#### OP_IfNot

This opcode will jump to `p2` if `p1` (or `p3` if `p1` is null) is non-zero.

#### OP_Insert

So this is the opcode responsible for inserting new records into a table. Now looking at this case, most of the work for inserting the record is in the `sqlite3BtreeInsert`.

#### OP_Integer

So this opcode is to effectively set the `p2` register equal to the integer in `p1`.

#### OP_Int64

So the purpose of this opcode is to set the contents of the `P2` register, equal to the 64-bit Integer in the `P4` register.

#### OP_IsNull

This opcode is another conditional. If the value in `P1` is null, than there is a jump to `P2`.

#### OP_MakeRecord

So the purpose of this opcode is to construct a record for a db, from specific column values.

#### OP_Ne

This opcode runs a conditional. If the conditional is true, then `p2` is jumped to. The two values being compared are `p3` and `p1`. The conditional here is not equal.

#### OP_Next

This opcode is responsible for moving the cursor of the table, to the next record in the table (the `BtCursor`). It primarily does this through the use of the `sqlite3BtreeNext` function (which wraps the `btreeNext` function). When this function moves the cursor up a single record in a table, all it appears to do is increment the `ix` value by one. This is an index value which indicates the current record of the table.

#### OP_NewRowid

So this opcode is to generate a new row id (value which uniquely identifies a specific row). It does this one of two ways. The first (which is the default), is grab the largest used rowid, and add one to it. If the largest row id is the largest value for the data type, then it randomly generates row ids until it finds one that hasn't been used.

#### OP_NULL

This opcode will set registers equal to Null. It will always set `P2` to Null.

#### OP_OpenEphemeral

This opcode will open a new cursor to a transient table.

#### OP_OpenRead

The purpose of this opcode is to open a Cursor to read from a table.

#### OP_OpenWrite

The purpose of this opcode is to open a Cursor to write from a table.

#### OP_ParseSchema

So this is a bit of an interesting opcode. This will read and parse all of the entries from the schema table of the database `P1`, that matches the where clause from `P4`. The thing about this opcode is, it will actually make another virtual machine, which recursively runs inside of this.

#### OP_ReadCookie

So this opcode is to read a cookie from a database. The cookie number will be specified with the `P3` register. The database is specified by the `P1` register. The output is stored in the `P2` register, which is the value of the cookie.

#### OP_ResultRow

This opcode is responsible for aggregating individual column values for a row, into a single row. It does this via iterating through each column, calling `Deephemeralize` on it, then jumping to `vdbe_return`.

#### OP_Rewind

This opcode will rewind the cursor to point to the first rowid of the table. It will configure it so as the cursor iterates through the table, it will go from the first row to the last name.

#### OP_Rowid

So this opcode, is to retrieve the rowid from the current record, which is specified in the `P1` register.

#### OP_SeekRowid

This opcode will search for a rowid. This rowid is specified in `P3`. The index of the cursor, is specified in `P1`. If it does have the record, the cursor is left pointing at that record and execution continues. If it doesn't then the instruction in `P2` is jumped to.

#### OP_SetCookie

So this opcode is to set a value for a cookie, in a database. `P3` is the value, `P2` is the cookie number, and `P1` is the database.

#### OP_String8

So this opcode is to set the `p2` register equal to a pointer to a UTF-8 string stored as a ptr in `p4` register.

## Code Flow

So the basic code flow of the program contains these three parts (although the first really isn't that long compared to the other):

```
0.) Scan in Sql query
1.) String Processing / vdbe code generation
2.) vdbe code execution
```

String processing and vdbe code execution have their own sections.

So at first the query is scanned into memory. This happens with `fgets` in the `one_input_line` function called from `process_input` (which is called from main). There is basically an infinite loop which happens within `process_input` that will continually scan in and process commands.

Now there are two separate types of commands. There are meta commands, which specify sql settings. These are specified via having the first character be a `.`. These commands are handles with the `do_meta_command` function. All queries that do not begin with a `.` are assumed to be a sql command, and are handled with the `runOneSqlLine` function.
