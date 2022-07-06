# Index

So an index is a seperate datastructure that stored metadata about a table. It is used to store information which can speed up queries:

```
sqlite> .open x
sqlite> CREATE TABLE x (y int, z varchar);
sqlite> CREATE INDEX index_x ON x(y);

.	.	.

sqlite> SELECT * FROM x;
5|55555
6|66666
7|77777
```

Now to see how some of the functionallity will change. We will run this query:

```
sqlite> SELECT * FROM x WHERE y > 5;
```

Which will translate to these opcodes:

```
0x3e: OP_Init
0x02: OP_Transaction
0x0b: OP_Goto
0x61: OP_OpenRead
0x61: OP_OpenRead
0x45: OP_Integer
0x19: OP_SeekGT
0x88: OP_DeferredSeek
0x5a: OP_Column
0x5a: OP_Column
0x51: OP_ResultRow
0x05: OP_Next
0x88: OP_DeferredSeek
0x5a: OP_Column
0x5a: OP_Column
0x51: OP_ResultRow
0x05: OP_Next
0x44: OP_Halt
```

So when we look at the opcodes, we see there are two `OP_OpenRead` opcodes. This is because it has to open up both the table, and the index associated with the table. Proceeding that it uses the `OP_Integer` opcode in order to store the integer we are comparing against in the `P2` regitser. Proceeding that, the `OP_SeekGT` opcode is used. This is used to position the cursor on the index, to the least value that is greater than the specified value, and configure the cursor to move in ascending order. That way in order to get all of the values, it will just have to move that cursor forward by one every time. Proceeding that, it will use the `OP_DeferredSeek` opcode to get the corresponding record in the actual table for the index row. Then it will follow the normal `OP_Column/OP_ResultRow` extraction method, before using `OP_Next` to move to the next record in the index.

So the first major functionallity that happens with the index is `OP_SeekGT`. Now the opcode for this, is bundled with other opcodes like `OP_SeekLT/OP_SeekLE/OP_SeekGE` (ops for finding greater/lesser than/equal). Looking at the code for this opcode, we can see it really has two parts. The first part is if it is dealing with a table, the second is if it is dealing with an index. To deal with an index, we see that it first calls the `sqlite3BtreeMovetoUnpacked` function to find the desired value for the compare, or the spot it should be if it doesn't exist. If it is a greater opcode `OP_SeekGE/OP_SeekGT`, it will then call `sqlite3BtreeNext` to find the next spot. For less than opcodes `OP_SeekLT/OP_SeekLE`, it will instead call `sqlite3BtreePrevious`.

So looking at the `sqlite3BtreeMovetoUnpacked`, we see that it will call the `moveToRoot` function to move the BtCursor of the table to the root page. Proceeding that, it will enter into an infinite loop (`for(;;)`). The loop will start by getting the current cell with the `findCellPastptr` function (this is an inlined function, that basically just grabs the offset from the header), with `idx` holding the index. There are three branches for the analysis, and which one is taken is determined by the first two bytes of the cell. If the first two branches are taken, it will run the function stored as `xRecordCompare` (which I've seen it be `vdbeRecordCompareInt`). The third branch appears to deal with cells that extend onto overflow pages.

For the `OP_DeferredSeek` opcode, which is responsible for changed the table cursor to point to the corresponding row from the index cursor. It effectively just sets the row idx of the table equal to the idx of the index cursor, using the `sqlite3VdbeIdxRowid` function to see what row the index cursor is currently pointing to.
# Index

So an index is a separate data structure that stores metadata about a table. It is used to store information which can speed up queries:

```
sqlite> .open x
sqlite> CREATE TABLE x (y int, z varchar);
sqlite> CREATE INDEX index_x ON x(y);

.    .    .

sqlite> SELECT * FROM x;
5|55555
6|66666
7|77777
```

Now to see how some of the functionality will change. We will run this query:

```
sqlite> SELECT * FROM x WHERE y > 5;
```

Which will translate to these opcodes:

```
0x3e: OP_Init
0x02: OP_Transaction
0x0b: OP_Goto
0x61: OP_OpenRead
0x61: OP_OpenRead
0x45: OP_Integer
0x19: OP_SeekGT
0x88: OP_DeferredSeek
0x5a: OP_Column
0x5a: OP_Column
0x51: OP_ResultRow
0x05: OP_Next
0x88: OP_DeferredSeek
0x5a: OP_Column
0x5a: OP_Column
0x51: OP_ResultRow
0x05: OP_Next
0x44: OP_Halt
```

So when we look at the opcodes, we see there are two `OP_OpenRead` opcodes. This is because it has to open up both the table, and the index associated with the table. Proceeding that it uses the `OP_Integer` opcode in order to store the integer we are comparing against in the `P2` regitser. Proceeding that, the `OP_SeekGT` opcode is used. This is used to position the cursor on the index, to the least value that is greater than the specified value, and configure the cursor to move in ascending order. That way in order to get all of the values, it will just have to move that cursor forward by one every time. Proceeding that, it will use the `OP_DeferredSeek` opcode to get the corresponding record in the actual table for the index row. Then it will follow the normal `OP_Column/OP_ResultRow` extraction method, before using `OP_Next` to move to the next record in the index.

So the first major functionallity that happens with the index is `OP_SeekGT`. Now the opcode for this, is bundled with other opcodes like `OP_SeekLT/OP_SeekLE/OP_SeekGE` (ops for finding greater/lesser than/equal). Looking at the code for this opcode, we can see it really has two parts. The first part is if it is dealing with a table, the second is if it is dealing with an index. To deal with an index, we see that it first calls the `sqlite3BtreeMovetoUnpacked` function to find the desired value for the compare, or the spot it should be if it doesn't exist. If it is a greater opcode `OP_SeekGE/OP_SeekGT`, it will then call `sqlite3BtreeNext` to find the next spot. For less than opcodes `OP_SeekLT/OP_SeekLE`, it will instead call `sqlite3BtreePrevious`.

So looking at the `sqlite3BtreeMovetoUnpacked`, we see that it will call the `moveToRoot` function to move the BtCursor of the table to the root page. Proceeding that, it will enter into an infinite loop (`for(;;)`). The loop will start by getting the current cell with the `findCellPastptr` function (this is an inlined function, that basically just grabs the offset from the header), with `idx` holding the index. There are three branches for the analysis, and which one is taken is determined by the first two bytes of the cell. If the first two branches are taken, it will run the function stored as `xRecordCompare` (which I've seen it be `vdbeRecordCompareInt`). The third branch appears to deal with cells that extend onto overflow pages.

For the `OP_DeferredSeek` opcode, which is responsible for changed the table cursor to point to the corresponding row from the index cursor. It effectively just sets the row idx of the table equal to the idx of the index cursor, using the `sqlite3VdbeIdxRowid` function to see what row the index cursor is currently pointing to.
