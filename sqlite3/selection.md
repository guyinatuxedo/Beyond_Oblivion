# Selection

This will cover the process of retrieving data from a database, through the use of a select statement. This is the query:

```
sqlite> SELECT * FROM x;
```

Which outputs these values:

```
0|00000
0|00000
0|00000
```

Which is executed with these different opcodes.


```
0x3e(62)    :    OP_Init
0x02(02)    :    OP_Transaction
0x0b(11)    :    OP_Goto
0x61(97)    :    OP_OpenRead
0x25(37)    :    OP_Rewind
0x5a(90)    :    OP_Column
0x5a(90)    :    OP_Column
0x51(81)    :    OP_ResultRow
0x05(05)    :    OP_Next
0x5a(90)    :    OP_Column
0x5a(90)    :    OP_Column
0x51(81)    :    OP_ResultRow
0x05(05)    :    OP_Next
0x5a(90)    :    OP_Column
0x5a(90)    :    OP_Column
0x51(81)    :    OP_ResultRow
0x05(05)    :    OP_Next
0x44(68)    :    OP_Halt
```

So we will start going through the opcodes. First is the `OP_OpenRead` opcode. This opcode will open up a cursor to the table, that is writable. This will do so through the use of a call of `sqlite3BtreeCursor`, which calls `btreeCursor`.

Proceeding that, it will call the `OP_Rewind` opcode. This opcode will move the cursor to the first entry. This is typically done with the `sqlite3BtreeFirst` function.

Now the remaining opcodes form a loop. It is a series of `OP_Column` opcodes, followed by the `OP_ResultRow` and `OP_Next` opcodes. This is repeated multiple times until execution is halted with a `OP_Halt` opcode. Each iteration of this pattern is to get a single row of results. The reason why this pattern only repeats three times is because the table only has three records, which will be retrieved with this query.

So the next opcode that is executed is `OP_Column`. This opcode is responsible for retrieving a single data field from a single record (which is why there are two `OP_Column` opcodes per record, since there are two fields being retrieved per record). This opcode is where the primary data retrieval work is executed. Also the majority of the logic does occur within the opcode itself, and is not passed to another function call.

So looking at the code for that opcode. We first see there is a check that happens, that the size of the row is not less than the defined header length. If that check is failed, then it checks if the header length is greater then 98307, or bigger than the defined payload size, and if it is then it flags it for corruption.

Now the part that happens after that check, is when it calculates where exactly the data field is. It will do this via a loop, where it iterates through the header values. The specific column that is trying to be extracted is specified in the `P2` register. It will use the header values to determine where the `P2-ith` value will be, using a do while loop. The offsets are stored in the `aOffset` array. It doesn't calculate offsets to data values past the data field it is trying to find (starts from the 1st).

There are several different checks that happen. It checks that it did not parse the header past the end of it, that the entire header was used by not the entire data section, or that the data offset doesn't extend past the end of the record. Then after that, it will actually extract the value. For numeric data types, this is typically done with the `sqlite3VdbeSerialGet` function. For string data types, this is typically done with `memcpy`.

Proceeding that there is the `OP_ResultRow` opcode, which will just construct the record to output from the values extracted from `OP_Column` opcodes.

After that, there is the `OP_Next` opcode, which will move the cursor to the next record. This is done with the `sqlite3BtreeNext` function (stored in `xAdvance`).
