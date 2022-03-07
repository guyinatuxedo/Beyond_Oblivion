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

So starting off, it will hash the key, to get the primary and secondary hashes. It will then use the primary hash to lookup the bucket offset in the bucket array. It will then attempt to read the data at the offset, as a record.

Proceeding that, it will check if the secondary hashes are equivalent. If not, it will check if there is a child for that hash, and iterate down the bucket tree accordingly. Once it finds a record with a corresponding secondary hash, it will compare the keys to see if they are equivalent. If they are, it will scan the value of the record and return it.

## Record Insertion



## Record Deletion