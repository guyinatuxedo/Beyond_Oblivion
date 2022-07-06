# Record Overflow

So this is another trick, that can be done via editing the database file to cause "interesting" behavior with the records. This was on sqlite3 version `3.35.1`. The purpose of this is we can overflow a record into subsequent records via overwriting the size value of the record to be larger, then updating it.

# Explannation

So when a record is updated, what happens is that record is removed and then a new record (the updated record) is inserted. We will first overwrite the size value of the record to extend into subsequent records. We will then update that record. That record will be freed, and the recorded free spot will extend into the next block. Then in the update process, a new record will be inserted into that spot (we will need to groom the heap correctly, and have the sizes match up). When the new record is inserted, it will extend into the next record, if the sizes are right.

# Example

So we start off with creating a table, with a few entries:

```
sqlite> .open example
sqlite> CREATE TABLE x (y int, z varchar);
sqlite> INSERT INTO x (y, z) VALUES (5, "55555");
sqlite> INSERT INTO x (y, z) VALUES (6, "66666");
sqlite> INSERT INTO x (y, z) VALUES (7, "77777");
sqlite> INSERT INTO x (y, z) VALUES (8, "88888");
sqlite> SELECT * FROM x;
5|55555
6|66666
7|77777
8|88888
```

Now, we will "corrupt" the database file. We will be editing the `6` record to overflow into the `5` record. This what the bytes of those records look like:

```
09 	02 	03 	01 	17 	06 	36 	36 	36 	36 	36
09 	01 	03 	01 	17 	05 	35 	35 	35 	35 	35
```

So we are going to change two values of the `6` record. Our objective is to overwrite the entirety of the `5` record. We will do this via updating the `6` record, to cover both the `6` and `5` records. We will accomplish this by expanding the size of the `6` record, to encompass both records. To do that we will chage the size from `0x09` to `0x14`, and the size of the varchar from `0x17` to `0x2D` (`hex(int((0x2D - 12) / 2)) = 0x10`). The `6` record will now look like this:

```
14 	02 	03 	01 	2D 	06 	36 	36 	36 	36 	36
```

Now to actually do the update:

```
sqlite> .open example_postupdate
sqlite> SELECT * FROM x;
5|55555
6|66666	55555
7|77777
8|88888
sqlite> UPDATE x SET z = "000000000000000" WHERE y = 6;
sqlite> SELECT * FROM x;
Error: database disk image is malformed
```
When we take a look at the database file, we see this. As we see, we have overwritten the record:

```
00 	13 	02 	03 	01 	2B 	06 	30 	30 	30 	30 	30 	30 	30 	30 	30 	30 	30 	30 	30
```
