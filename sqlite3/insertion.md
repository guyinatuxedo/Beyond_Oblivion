# Insertion

So this document will discuss the process for inserting a cell.

So starting off, let's create the table:

```
sqlite> CREATE TABLE x (y int, z varchar);
```

Now we will insert a record using this sql query:

```
sqlite> INSERT INTO x (y, z) VALUES (0, "00000");
```

Now this will be converted into the following opcodes:

```
062:    OP_Init
002:    OP_Transaction
011:    OP_Goto
098:    OP_OpenWrite
069:    OP_Integer
116:    OP_String8
122:    OP_NewRowid
092:    OP_MakeRecord
123:    OP_Insert
068:    OP_Halt
```

Now for what these opcodes do. The first three opcodes are executed at the start of most operations I've seen.

The first opcode that we will really talk about is `OP_OpenWrite`. This opcode will open up a cursor to the table, that is writable. This will do so through the use of a call of `sqlite3BtreeCursor`, which calls `btreeCursor`.

Now the next three opcodes `OP_Integer`, `OP_String8`, and `OP_NewRowid` serve effectively the same purpose. The record will have three values in it. These three opcodes will prepare the three values for the record. The integer value is the `0`, and the string value is the `"00000"` that we specified from the insertion query. The third value is the id of the row, which is generated and stored even though we really don't see it from the end user perspective. Looking at that opcode, we can see it generates a row id either one of two ways. This depends on if random row id has been enabled, which in this case it hasn't. If random row id is diabled, then it uses the `sqlite3BtreeLast` function to move the cursor to the last record, which should have the highest value row id (since it goes from `1` to `2`, and so on and so forth). Then it uses the `sqlite3BtreeIntegerKey` function to get that value, and increments it by `1`.

Now for the `OP_MakeRecord` opcode. This will actually construct the record to be inserted into the table. This entails several different parts. The first major part is when it calculates the size of the record. This is done in a loop that iterates from the last value to the first. Now there are effectively two different types of values, integers and strings. For integer values it checks what type of integer it can be stored as via seeing if it lower than a particular value. If it is lower than that value, then it knows it can be stored in that integer type (types being like `1` byte int, `2` byte in, `8` byte int). Also the type of value is specified via setting the `uTemp`. For string values, it calculates the necessary size via using the size specified in the data field. Now as it does this, there are two size values for each field. The size to store the data, and the size to specify it in the header, which is calculated via a call to `sqlite3VarintLen` with the datatype as the argument. Then proceeding that it will increment the size of the header to compensate for the total header size value. Then it will check that the output buffer is large enough to hold the record, and resize it if it's not. Then it will construct the record via placing the values in the record (`putVarint32` for the header values and `sqlite3VdbeSerialPut` for the actual data values), in a loop that iterates from the first value to the last.

Now for the final opcode that runs of real significance to this document, which is `OP_Insert`. This actually inserts the record into the table. This does so through the use of the `sqlite3BtreeInsert`, which usually then calls `insertCell` to actually insert it. This does so through allocating space on the page with the use of a `allocateSpace` call, and then uses `memcpy` to write the record to the table.

Lastly there is `OP_Halt` which is just the generic opcode to halt execution.


For more info, checkout: https://www.youtube.com/watch?v=7u942Xgc26A&t=1553s