# Record Overlaping

So this is another trick that can come about by editing the database file. This will lead to allocation of overlapping records.

## Explannation

So this deals with the space allocation for new records. Specifically by overwriting the `OffsetContent` value of a page header. We overwrite it to point to to inside of the records itself. This will cause it to allocate the next record ontop of existing records.

## Example

So take for instance, we have the `overlaping_records_example` file with these records:

```
sqlite> .open overlaping_records_example
sqlite> CREATE TABLE x (y INT, z VARCHAR);
sqlite> INSERT INTO x (y, z) VALUES (0, "0");
sqlite> INSERT INTO x (y, z) VALUES (1, "1");
sqlite> INSERT INTO x (y, z) VALUES (2, "2");
sqlite> select * from x;
0|0
1|1
2|2
sqlite> .exit
```

For this, the database file looks like this:

The header/ptrs array:
```
0D	00	00	00	03	0F	ED	00	0F	FA	0F	F4	0F	ED
```

So here, we can see that this page holds three records at offset `0x0FED`, `0x0FF4`, and `0x0FFA`. We can also see that the Content Offset is `0x0FED`. We will overwrite this to be `0x0FF3`. This file will be the `overlaping_records_example_corrupted_preinsert`. This way, the start of the content section is at the start of the record 2, the next record will be allocated (since there are no free spots) ontop of record with `2`.

```
0D	00	00	00	03	0F	F3	00	0F	FA	0F	F4	0F	ED
```

Then when we insert a new record:

```
sqlite> .open overlaping_records_example_corrupted_insert
sqlite> select * from x;
0|0
1|1
2|2
sqlite> INSERT INTO x (y, z) VALUES (0, "0");
sqlite> select * from x;
0|0
1|1
0|0
0|0
sqlite> .exit
```

When we take a look at the header, we see that there is overlapping records. This is also apparant, since we see that the `2` record got changed to `0`, from the select:

```
0D	00	00	00	04	0F	ED	00	0F	FA	0F	F4	0F	ED	0F	ED
```

When we look at the records, we see that there are only three in existence. 

```
04	04	03	08	0F	30	32	04	02	03	09	0F	31	04	01	03	08	0F	30
```