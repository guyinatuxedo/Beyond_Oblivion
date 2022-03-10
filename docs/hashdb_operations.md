# HashDB Operations

So this file will cover how the database will actually perform these operations:
```
Record Lookup
Record Insertion
Record Deletion
```

## Record Lookup

```
0.)	Hash the key
```

So starting off, it will hash the key, to get the primary and secondary hashes (secondary hash is used to uniquely identify keys where hash collosions occur witht he primary hash). It will then use the primary hash to lookup the bucket offset in the bucket array. It will then attempt to read the data at the offset, as a record.

Proceeding that, it will check if the secondary hashes are equivalent. If not, it will check if there is a child for that hash, and iterate down the bucket tree accordingly. Once it finds a record with a corresponding secondary hash, it will compare the keys to see if they are equivalent. If they are, it will scan the value of the record and return it.

## Record Insertion

So for starting off, it will hasht the key just like for record insertion. It will then attempt to look up the offset to the bucket, in the bucket array. Now, this is where the insertion process will fork off into different methods. This depends on what it finds in the bucket, and here are the conditions:

```
0.)	There is no bucket offet
1.) There are records in the bucket, but none with the same key
2.) There are records in the bucket, with the same key
```

#### No Bucket Offset

So this condition will occur when we are inserting a record, that the bucket doesn't actually have any records, so the bucket hasn't been made yet. For this method, it will attempt

#### Records in bucket, with different key

#### Records in bucket, with same key


## Record Deletion