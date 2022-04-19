
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

So int's come in one of two datatypes, small and full ints. The difference between these two is which bits of the int are set. If the upper three bits are set, it is a full int. If not, it is a small int. We see it from this Macro, and constant declaration:

```
./whitedb.h:#define fits_smallint(i)   ((((i)<<SMALLINTSHFT)>>SMALLINTSHFT)==i)

.	.	.

./whitedb.h:#define SMALLINTSHFT  3
```

So for these two datatypes, the primary difference in how they are stored is this. For smallints, the actual int value is encoded, then stored in the column. For fullints, space is allocated using the custom memory allocation functionallity of the database. The int is stored at the space, and the offset to that space (encoded first) is stored in the column. So tl;dr, for small ints the value is store, for full ints a ptr is stored.

We see that to encode a small int, it just shifts it over to the left by `SMALLINTSHFT`, than ors it with `SMALLINTBITS`. THis is so it can check if it is a small int via seeing if the bits that correspond to `SMALLINTBITS` are set:

```
../databases/whitedb/whitedb.h:#define encode_smallint(i) (((i)<<SMALLINTSHFT)|SMALLINTBITS)
```

Which we see that in action right here:

```
../databases/whitedb/whitedb.h:#define issmallint(i)   (((i)&SMALLINTMASK)==SMALLINTBITS)
```

And we see the constants here:

```
../databases/whitedb/whitedb.h:#define SMALLINTMASK  0x7

.	.	.

../databases/whitedb/whitedb.h:#define SMALLINTBITS    0x3       ///< int ends with       011

.	.	.

../databases/whitedb/whitedb.h:#define SMALLINTSHFT  3
```

So for full ints, it allocates the data, and encodes it:

```
    offset=alloc_word(db);

.	.	.

    return encode_fullint_offset(offset);
```

Which when we look at how it encodes it, it just ors the value with `FULLINTBITS` to mark it as a full int:

```
../databases/whitedb/whitedb.h:#define encode_fullint_offset(i) ((i)|FULLINTBITS)
```

Which we see it as just `0x1`:

```
../beyond_oblivion/whitedb/database_format.md:./whitedb.h:#define FULLINTBITS  0x1      ///< full int ptr ends with       01
```

And we see, it's used to identify it as a full int, similar to a small int:

```
../databases/whitedb/whitedb.h:#define isfullint(i)    (((i)&FULLINTMASK)==FULLINTBITS)
```

#### char

So chars as a datatype, are similar to that of small ints. When they are stored in a field (which I also call a column), their actual value is stored in there. We see that the `wg_encode_char` function basically just wraps the `encode_char` macro:

```
wg_int wg_encode_char(void* db, char data) {
#ifdef CHECK
  if (!dbcheck(db)) {
    show_data_error(db,"wrong database pointer given to wg_encode_char");
    return WG_ILLEGAL;
  }
#endif
#ifdef USE_DBLOG
/* Skip logging values that do not cause storage allocation.
  if(dbh->logging.active) {
    if(wg_log_encode(db, WG_CHARTYPE, &data, 0, NULL, 0))
      return WG_ILLEGAL;
  }
*/
#endif
  return (wg_int)(encode_char((wg_int)data));
}
```

Which we see, just shifts the chart over, and ors it with `CHARBITS` so it can be identified as a char. Since chars should only be `0x08` bits, we don't have to worry about it being too big to store in the field, like with ints:

```
../databases/whitedb/whitedb.h:#define encode_char(i) (((i)<<CHARSHFT)|CHARBITS)
```

Which we see the values for the shift, and the actual char bits used to identify it here:

```
../databases/whitedb/whitedb.h:#define CHARSHFT  8

.	.	.

../databases/whitedb/whitedb.h:#define CHARBITS  0x1f       ///< char ends with 0001 1111

.	.	.

../databases/whitedb/whitedb.h:#define CHARMASK  0xff
```

Which we see here, it is used to actually identify it:

```
../databases/whitedb/whitedb.h:#define ischar(i)   (((i)&CHARMASK)==CHARBITS)
```

#### string

#### record

```
0xB770
0xB7A0
0xB7D0
```





