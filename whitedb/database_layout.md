
Header

```
0x00	-	4 byte constant 0x4973b223
0x04 	-	4 byte version
0x08 	-	4 byte features
0x0c 	-	4 byte CRC32 checksum
0x10	-	4 byte size
0x14	-	4 byte first free area ptr
0x18	-	
0x1c	-	
0x20	-	4 byte datarec area header
0x24 	-	4 byte longstr area header
0x28	-	4 byte listcell area header
0x2c	-	4 byte shortstr area header
0x30	-	4 byte word area header
0x34 	-	4 byte doubleword area header
```




## Record Structure

So records are stored as arrays of `gints`, which are `0x08` byte values. Now there is no set schema for the records in a database, and two records can have a different number of values, with different types.

Now a record will have `0x03` gints which for it's header:

```
#define RECORD_HEADER_GINTS 3
```

### Value Encodings

So past that, the rest of the gints form the actual columns/values of the record. Now here are some of the potential dataypes a record can hold, here are only three of them:

```
int
char
string
record (ptr to another record)
```

Now typically, when a value is being stored, it is stored as one of two ways. If the value can actually be stored in the `0x08` byte slot, it will store it there. However if it can't, it will allocate a chunk of memory, and store the offset to it in the record. So it effectively either stores the value, or a ptr to the value.

Also, there are a ton of different datatypes, which can be stored in any slot in a record. How it tells the type of a value in a record, is via setting certain bits of the value stored in the record. For instance, if the lower `2` bits (so `0x03`) are set, that means that it is a small int, which we see from these values:

```
../whitedb.h:#define SMALLINTBITS    0x3       ///< int ends with       011

.	.	.

../whitedb.h:#define SMALLINTMASK  0x7
```

And this macro:

```
../whitedb.h:#define SMALLINTMASK  0x7
```

Also because of this, there are typically encoding/decoding values that are stored. That way you can take a value, encode it, store the encoded value, then when you retrieve that value, because of the encoding, you can tell what type it is, decode it, and then get the original value.

#### int

So int's are

#### char

#### string

#### record

```
0xB770
0xB7A0
0xB7D0
```





