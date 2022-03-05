# HashDB

So this document describes the database format of a hash database.

So the database starts off with a header, with these offsets:

```
0x00:	Database Name/Version String
0x20:	HDBTYPEOFF		-	database type
0x21:	HDBFLAGSOFF		-	addiitonal flags
0x22:	HDBAPOWOFF		-	Power of record alignmend
0x23:	HDBFPOWOFF		-	Power of free block pool number
0x24:	HDBOPTSOFF		-	Options
0x28:	HDBBNUMOFF		-	Number of the Bucket Array
0x30:	HDBRNUMOFF		-	Number of records
0x38:	HDBFSIZOFF		-	Size of the Database File
0x40:	HDBFRECOFF		-	Offset to the first record


0x40:	RECORDS_OFFSET	-	Offset to records
```

Hashing Algorithm:
```
def hash_offset(inp):
	hash_val = 19780211
	bucket_number = 0x1ffff
	for i in inp:
		hash_val = ((hash_val * 37) + ord(i)) & 0xffffffffffffffff
	return_value = ((hash_val % bucket_number) * 0x4) + 0x100
	return return_value
```

Now in the database records section, it begins at the offset specied by the `RECORDS_OFFSET`. Now it starts off with a record. Records have the following format:

```
MAGIC_VALUE
PADDING
NEXT_PADDING_SIZE
NULL_BYTE
HASH_SIZE
VALUE_SIZE
```

MAGIC_VALUE - 

PADDING - Null bytes between `MAGIC_VALUE` and `NEXT_PADDING_SIZE`

## Hashing

So this database operates off of a hashmap. The hashmap will map a particular key, to a bucket. A bucket is a collection that holds various values, with a value basically being a database record. Now since multiple keys can map to a single bucket, is the reason why a bucket needs to be able to hold multiple records (There are far more hashes that could be used, versus buckets to store it in). Now there is an array of buckets, and the output of the hashing algorithm will effectively serve as an index to that array, and whatever bucket comes up is the bucket which holds the record.

Now the hashing algorithm will hash the key, which is a string. Below is a python3 function to actually hash a key:

```
def hash_offset(inp):
	hash_val0 = 19780211
	bucket_number = 0x1ffff
	for i in inp:
		hash_val0 = ((hash_val0 * 37) + ord(i)) & 0xffffffffffffffff
	hash_val0 = (hash_val0 % bucket_number)
	print("Hash Value 0: " + hex(hash_val0))

	hash_val1 = 751
	for i in reversed(inp):
		hash_val1 = ((hash_val1 * 31) ^ ord(i)) & 0xffffffff
	hash_val1 = hash_val1 & 0xff
	print("Hash Value 1: " + hex(hash_val1))


hash_offset("gohan")
hash_offset("picolo")
hash_offset("vegeta")
```
Now looking at the script, we see there are two seperate hash values, `0` and `1`. That is because the hashing algorithm the database uses will actually generate two seperate hash values. One is used as an index into the bucket array, the other is stored in the record itself, for checking purposes. The `hash_val0` value is used for the bucket array index, and `hash_val1` is used as the checking hash.

Now to actually get the offset into the database file, it will typically that that index, multiply it by `4` (because that is the size of the integer), and add `0x100` to it (start of the bucket index). One thing to note, if the buckets are `64` bit, it may be multiplied by `8` (unsure tbh). Here is a python3 file which will actually give us the offset to the bucket:

```
def hash_offset(inp):
	hash_val = 19780211
	bucket_number = 0x1ffff
	for i in inp:
		hash_val = ((hash_val * 37) + ord(i)) & 0xffffffffffffffff
	return_value = ((hash_val % bucket_number) * 0x4) + 0x100
	return return_value

print(hex(hash_offset("gohan")))
print(hex(hash_offset("picolo")))
print(hex(hash_offset("vegeta")))
```

Now when you actually look at that offset in the database file, you will see a 4 byte integer. Now this integer, when shifted, will actually be the offset to the bucket itself. We see in the `tchdbgetbucket` function, that it is shifted to the left by `hdp->apow`:

```
/* Get the offset of the record of a bucket element.
   `hdb' specifies the hash database object.
   `bidx' specifies the index of the bucket.
   The return value is the offset of the record. */
static off_t tchdbgetbucket(TCHDB *hdb, uint64_t bidx){
  assert(hdb && bidx >= 0);
  if(hdb->ba64){
    uint64_t llnum = hdb->ba64[bidx];
    return TCITOHLL(llnum) << hdb->apow;
  }
  uint32_t lnum = hdb->ba32[bidx];
  return (off_t)TCITOHL(lnum) << hdb->apow;
}
```

Now in practice, most of the time, I see the value for `hdb->apow` be `0x4`. 

## Buckets

## Database Open

The process of opening up a new hash database is done with the use of two functions, `tchdbnew` to make a new Hash DB object, and `tchdbopen` to actually handle the process of opening up the database file. The `tchdbnew` function just appears to simple malloc a new db object struct, then call `tchdbclear` to clear it out (`bzero` it out effectively).

So the `tchdbopen` function appears to be a wrapper for the `tchdbopenimpl`. Looking at the `tchdbopenimpl` function, we can see it starts off by opening up the database file. 

So the `tchdbdumpmeta` function is responsible for writing the database meta data to a buffer, to be written to the database file. We can effectively see what the header is. 

## Record Insertion

So the process of inserting a record is done through the use of the `tchdbput2` function (among others). This function takes three arguments, the hash database object, the key, and the value.

## Record Lookup

## Database Close
