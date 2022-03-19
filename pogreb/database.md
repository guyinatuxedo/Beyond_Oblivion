## Files

So this database will store it's data within several files, contained within a folder named after the database. Here is a list of them:

```
00000-1.psg 		-	Stores the actual data for a segment (so the records)
00000-1.psg.pmt 	-	Stores the metadata for the corresponding segment
db.pmt 				-	Stored Database Meta Information
index.pmt 			-	Stores Index Metadata
main.pix 			-	Stores Actual indices
overflow.pix 		-	Stores Index Overflow
```

So there are two main parts to this database, the datalog and the index. The datalog is what actually stores the data for a record, and the index is what stores information as to where the records are.

## Record Storage

## Index Storage

## Metadata Storage