# HashDB

## Database Format

So a HashDB will have three primary parts to it:

```
Database Header
Bucket Array
Records
```

Those three parts will be in that order. Below you will see documentation regarding these three parts.

## Database Header

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


0x40:	RECORDS_OFFSET	-	Offset to records section
```

## Bucket Array

So the bucket array region is fairly simple. It is effectively just an array of offsets. This region doesn't actually hold the buckets, or any of the records, those all are in the record section.

So how the database will find a particular record is this. It will take the key, hash it, use the primary hash from the hashing algorithm as an index into the bucket array, and grab the offset, and then find the bucket that way (then search through the bucket for the record).

One thing about the offsets in the bucket array, they aren't the actual offsets, rather they are shifted. We see this in several different spots in the code path:

Like getting the bucket:
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

Typically the value stored will be shifted to the right, in order to conserve space. In all instances I've seen, the shift value (`hdb->apow`) will be `4` bits. So an offset of `0x81140` will be stored as the value `0x8114`.

Also, here is how the hashing algorithm works.

#### Hashing

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

## Buckets/Records

So this section is basically just buckets an records (actually what holds the database data). First, let's go over the layout of a record:

```
MAGIC_VALUE
SECONDARY_HASH_VALUE
LEFT_CHILD_OFFSET
RIGHT_CHILD_OFFSET
PADDING_LENGTH
KEY_LENGTH
VALUE_LENGTH

KEY
VALUE
PADDING
```

So to go over these, first we will go over the record header values:
```
	*	MAGIC_VALUE - A one byte constant value, `0xc8`
	* SECONDARY_HASH_VALUE - A one byte value, derived from the hashing algorithm, used to identify this particular record when multiple records map to the same primary hash
	* LEFT_CHILD_OFFSET - The offset to the left child, 4 byte int
	* RIGHT_CHILD_OFFSET - The offset to the right child, 4 byte int
	* PADDING_LENGTH - Two byte int, representing the number of padding bytes at the end
	* KEY_LENGTH - A one byte int, representing the length of the key
	* VALUE_LENGTH - A one byte int, representing the length of the valu
```

And then, for the record body parts:
```
	*	Key - The key of the record
	* Value - The value of the record
	* Padding - Null Bytes padding the end, up until the next record, or end of database.
```

Now all a bucket really is, is just a binary search tree of the records. To search through a tree of records, it will use the secondary hash value. Starting from the first record (the one pointed to by the initial bucket record offset), it will compare the secondary hash it is searching for, to the one it sees in the record. If the hash it is searching for is less than the current record's hash, it will go to the right child. If the hash it is searching for is greater than, it will go to the left child. We see that code in the `tchdbgetimpl` function:

```
  while(off > 0){
    rec.off = off;
    if(!tchdbreadrec(hdb, &rec, rbuf)) return NULL;
    if(hash > rec.hash){
      off = rec.left;
    } else if(hash < rec.hash){
      off = rec.right;
    } else {
      if(!rec.kbuf && !tchdbreadrecbody(hdb, &rec)) return NULL;
```

Also just like the offsets in the bucket array, these are also shifted. Getting Left/Right record children (tchdbreadrec):

```
    uint64_t llnum;
    memcpy(&llnum, rp, sizeof(llnum));
    rec->left = TCITOHLL(llnum) << hdb->apow;
    rp += sizeof(llnum);
    memcpy(&llnum, rp, sizeof(llnum));
    rec->right = TCITOHLL(llnum) << hdb->apow;
    rp += sizeof(llnum);
```
