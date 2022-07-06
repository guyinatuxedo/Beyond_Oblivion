# Memory Layout

This page will include info on what the exact data structures in memory look like for Sqlite3

## Records

The basic format of a record looks like this (this is taken partially from of the one from the sqlite3 comments):

```
+---------------------------------------------------------------------------------------------------------------------------+
| total record size | record ID | header size | datype 0 | datatype 1 | ... | datatype n - 1 | data 0 | data 1 | data n - 1 |
+---------------------------------------------------------------------------------------------------------------------------+
```

Now here is a chart of the various datatype values:
```
00                    NULL
01                    1 byte signed int
02                    2 byte signed int
03                    3 byte signed int
04                    4 byte signed int
05                    6 byte signed int
06                    8 byte signed int
07                    IEEE float
08                    Integer constant 0
09                    Integer Constant 1
10                    reserved for future use
11                    reserved for future use
>= 12 (even)        BLOB
>= 13 (odd)            TEXT
```

Let's take a look at an example record:

```
gef➤  x/2g 0x5555556b8be2
0x5555556b8be2:    0x3339353101011d04    0x7bf00f0538323735
```

Which corresponds to this record:
```
select * from x;
15935728|5|15
```

So we can see that the `header size` is `0x04`. These `0x04` bytes include it, and the three data type bytes. The first datatype byte is `0x1d` (`29`), which corresponds to the `TEXT` datatype. Then there are two `0x01` bytes, which signify 1 byte signed ints. Proceeding that, we have the data of the record, we have the string `15935728`, followed by `0x05` (`5`) and `0x0f` (`15`). So this record can be shown as:

```
+---------------------------------------------------------------------------------------------------------------+
| header size 0x04 | datype0 0x1d | datatype1 0x01 | datatype2 0x01 | data0 "15935728" | data1 0x05 | data2 0xf |
+---------------------------------------------------------------------------------------------------------------+
```

Also one thing to note. The length of the string is calculated, by the datatype minus 12, divided by two. The length of our string is `8`, which we get from `(29 - 12) / 2 = 8` (remainder dropped).

Now one thing to take into account. Each record has a few bytes before it. The record ptr to this record, will point to the first of these bytes. In a lot of instances that I've seen, this will be two bytes. The first is the size of the record, the second appears to be a row id.

## MemPage

So sqlite uses a paging mechanism in order to handle it's memory. It will allocate a page, which will actually store the data for the database. The Pages have this structure (diagram is from comments):

```
+--------------------+
| File Header        |
+--------------------+
| Page Header        |
+--------------------+
| Cell Pointer Array |
+--------------------+
|                    |
| Unallocated Space  |
|                    |
+--------------------+
| Cell Content Area  |
+--------------------+
```
#### File Header

So this is the header for the database file. Sqlite3 stores the database in a database file that is written to disk. This header is only present on the root page. On the root page, it is present before the page header, and is the first thing on the page. It is `0x64` (`100`) bytes in size, with these values:

```
Offset         Size         Description
0x00         0x10         Header String "SQLite format 3"
0x10         0x02         Page Size in Bytes
0x12         0x01         File Format Write Version
0x13      0x01         File Format Read Version        
0x14         0x01         Bytes of unused space at the end of each page
0x15         0x01         
0x16         0x01         
0x17         0x01         Min Leaf payload fraction (has to be 32)
0x18         0x04         File Change Counter
0x1c         0x04         Reserved for future use
0x20         0x04         First Freelist Page
0x24         0x04         Number of freelist pages in the file
0x28         0x3c         15 different 4 byte values

0x28         0x04         Schema cookie
0x2c         0x04         File format of schema layer
0x30         0x04         Size of page cache
0x34         0x04         Largest root-page (auto/incr_vacuum)
0x38         0x04         1=UTF-8 2=UTF16le 3=UTF16be
0x3c         0x04         User version
0x40         0x04         Incremental vacuum mode
0x44         0x04         Application-ID
0x48         0x04         unused
0x4c         0x04         unused
0x50         0x04         unused
0x54         0x04         unused
0x58         0x04         unused
0x5c         0x04         The version-valid-for number
0x60         0x04         SQLITE_VERSION_NUMBER
```

#### Page Header

So this is the header for the Page itself. There are two sizes for this `0x08` bytes for leaf pages, and `12` bytes for interior pages. Pages are stored with B-Trees (google if you need a refresher). The reason for the difference in sizes is the leaf pages don't need the `RightChild` value.

```
Offset         Size         Name
0x00         0x01         Flags
0x01         0x02         OffsetFree
0x03         0x02         NumCells
0x05         0x02         OffsetContent
0x07         0x01         FragBytes
0x08         0x04         RightChild
```

The `Flags` value specifies certain flags. The flags are:

```
-    1    intkey                Signifies the key is an int which is stored in the key size entry of the cell header
-    2    zerodata            Signifies that this page only contains keys, no data
-    4    leafdata            
-    8    leaf                Signifies it is a leaf page (has no children)
```

The `OffsetFree` value specifies the byte offset to the first free block, to be allocated.

The `NumCells` value specifies the number of cells currently present on this cell.

The `OffsetContent` value specifies the byte offset to the content area.

The `FragBytes` value specifies the number of fragmented free bytes.

The `RightChild` value specifies the right child for the sub tree.

#### Cell Ptr Array

So this is an array, that consists of ptrs. These ptrs will point to individual cells, within the page. This space of memory will grow, and it will grow downwards, into the unallocated space.

It begins at the first byte after the page header. The ptrs themselves are 2 byte values, which signify the offset from the beginning of the page to the cell content in the cell content area. They occur in sorted order. The ptr to the first cell is first, the

#### Unallocated Space

This is the free space in the page itself, which new memory will be allocated from. The `Cell Ptr Array` region will grow downwards into it, and the `Cell Content Area` will grow upwards into it.

#### Cell Content Area

This is the area of the page, which the actual content of the cells will be stored. This region of memory will grow, and it will grow upwards into the `Unallocated Space`.

Space in the cell content area can be freed. These freed blocks are tracked with a linked list. Each block of free space is at least `0x04` bytes large. The free blocks have a header of `0x04` bytes, which starts at the beginning of the block. It contains these two values:

```
Offset         Size         Purpose
0x00         0x02         Byte offset to next freeblock
0x02         0x02         Number of bytes in this freeblock
```

Now there are blocks of free space that are less than `0x04` bytes large. These are known as fragments. The number of fragment bytes are stored in the page header value `FragBytes`.

#### Viewing Page in Memory

So let's take a look at one of these pages in memory. They are kept track of using the `MemPage` struct:

```

/*
** An instance of this object stores information about each a single database
** page that has been loaded into memory.  The information in this object
** is derived from the raw on-disk page content.
**
** As each database page is loaded into memory, the pager allocats an
** instance of this object and zeros the first 8 bytes.  (This is the
** "extra" information associated with each page of the pager.)
**
** Access to all fields of this structure is controlled by the mutex
** stored in MemPage.pBt->mutex.
*/
struct MemPage {
  u8 isInit;           /* True if previously initialized. MUST BE FIRST! */
  u8 bBusy;            /* Prevent endless loops on corrupt database files */
  u8 intKey;           /* True if table b-trees.  False for index b-trees */
  u8 intKeyLeaf;       /* True if the leaf of an intKey table */
  Pgno pgno;           /* Page number for this page */
  /* Only the first 8 bytes (above) are zeroed by pager.c when a new page
  ** is allocated. All fields that follow must be initialized before use */
  u8 leaf;             /* True if a leaf page */
  u8 hdrOffset;        /* 100 for page 1.  0 otherwise */
  u8 childPtrSize;     /* 0 if leaf==1.  4 if leaf==0 */
  u8 max1bytePayload;  /* min(maxLocal,127) */
  u8 nOverflow;        /* Number of overflow cell bodies in aCell[] */
  u16 maxLocal;        /* Copy of BtShared.maxLocal or BtShared.maxLeaf */
  u16 minLocal;        /* Copy of BtShared.minLocal or BtShared.minLeaf */
  u16 cellOffset;      /* Index in aData of first cell pointer */
  int nFree;           /* Number of free bytes on the page. -1 for unknown */
  u16 nCell;           /* Number of cells on this page, local and ovfl */
  u16 maskPage;        /* Mask for page offset */
  u16 aiOvfl[4];       /* Insert the i-th overflow cell before the aiOvfl-th
                       ** non-overflow cell */
  u8 *apOvfl[4];       /* Pointers to the body of overflow cells */
  BtShared *pBt;       /* Pointer to BtShared that this page is part of */
  u8 *aData;           /* Pointer to disk image of the page data */
  u8 *aDataEnd;        /* One byte past the end of usable data */
  u8 *aCellIdx;        /* The cell index area */
  u8 *aDataOfst;       /* Same as aData for leaves.  aData+4 for interior */
  DbPage *pDbPage;     /* Pager page handle */
  u16 (*xCellSize)(MemPage*,u8*);             /* cellSizePtr method */
  void (*xParseCell)(MemPage*,u8*,CellInfo*); /* btreeParseCell method */
};
```

Now this struct contains metadata describing the page. There are ptrs in this struct to the page in memory. The `aData` value contains a ptr to the start of the page. The `aDataEnd` ptr points to the next byte immediately following the end of the page. The `aCellIdx` ptr points to the `Cell Ptr Array` section of the page. Here is a `MemPage` struct in memory (passed as the first argument/$rdi register to the `insertCell` function). This is a leaf page:

```
gef➤  x/100g $rdi
0x5555556b9f20:    0x201010001    0xfdd00007f000001
0x5555556b9f30:    0xfc4000801e9    0xfff0004
0x5555556b9f40:    0x0    0x0
0x5555556b9f50:    0x0    0x0
0x5555556b9f60:    0x0    0x555555694550
0x5555556b9f70:    0x5555556b8ea0    0x5555556b9ea0
0x5555556b9f80:    0x5555556b8ea8    0x5555556b8ea0
0x5555556b9f90:    0x5555556b9ed8    0x555555589740
0x5555556b9fa0:    0x5555555895f0    0x66206574694c5153
0x5555556b9fb0:    0x332074616d726f    0x2020400001010010
0x5555556b9fc0:    0x20000001e000000    0x0
0x5555556b9fd0:    0x400000003000000    0x0
0x5555556b9fe0:    0x1000000    0x0
0x5555556b9ff0:    0x0    0x0
0x5555556ba000:    0x1e00000000000000    0xd784f2e00
0x5555556ba010:    0xca0f00ca0f01    0x0
0x5555556ba020:    0x0    0x0
0x5555556ba030:    0x0    0x0
0x5555556ba040:    0x0    0x0
0x5555556ba050:    0x0    0x0
0x5555556ba060:    0x0    0x0
```

Here are some values:

```
Name         Value
aData         0x5555556b8ea0
aDataEnd     0x5555556b9ea0
aCellIdx     0x5555556b8ea8
```

Now actually looking at the memory page iself (just the header for now), we see this:

```
gef➤  x/g 0x5555556b8ea0
0x5555556b8ea0:    0x00d40f040000000d
```

Which we can get the following header values from:
```
Name                     Value
Flags                     0x0d (Flags set : intKey, leafData, leaf)
offsetFree         0x0000
NumCells                 0x0004
OffsetContent             0x0fd4
FragBytes                 0x00
RightChild                 Not Present (leaf page)
```

So from this, we can tell that there are currently `4` cells. The region for the content of those cells starts at the offset `0xfd4`. In addition to that, this is a leaf page. Now let's take a look at the cell ptr region, which is at offset `0x08`:

```
gef➤  x/2g 0x5555556b8ea0+0x08
0x5555556b8ea8:    0xd40fe20fec0ff60f    0xa70fb20fbd0fc70f
```

So there are four cells. The ptrs are really just 2 byte int offsets from the start of the page. Now it appears there are more than `0x04` offsets in here. However only four are in use, the others are probably just old offsets left behind, not currently in use.

Now this means there are four cells, which are at these offsets:

```
Cell0 : 0x0ff6
Cell1 : 0x0fec
Cell2 : 0x0fe2
Cell3 : 0x0fd4
```

We see the offsets increase, because that region of memory grows upwards. Also we see that the highest cell offset `0x0fd4` matches the `OffsetContent` value, which makes sense (it is the closest cell to the top of the page, and the beginning of that region). Now looking at the records themselves:

```
gef➤  x/4g 0x5555556b8ea0+0x0ff6
0x5555556b9e96:    0x3232320213030108    0x5555556b8ea0de00
0x5555556b9ea6:    0x5555556b9ed80000    0x0001000000020000
gef➤  x/4g 0x5555556b8ea0+0x0fec
0x5555556b9e8c:    0x3232320213030208    0x320213030108de00
0x5555556b9e9c:    0x556b8ea0de003232    0x556b9ed800005555
gef➤  x/4g 0x5555556b8ea0+0x0fe2
0x5555556b9e82:    0x3232320213030308    0x320213030208de00
0x5555556b9e92:    0x13030108de003232    0x8ea0de0032323202
gef➤  x/4g 0x5555556b8ea0+0x0fd4
0x5555556b9e74:    0x393531011d03040c    0x0308053832373533
0x5555556b9e84:    0xde00323232021303    0x3232320213030208
```

I covered cells in an earlier section, so I won't dig into these ones. So in total, here is the page:

```
gef➤  x/512g 0x5555556b8ea0
0x5555556b8ea0:    0x00d40f040000000d    0xd40fe20fec0ff60f
0x5555556b8eb0:    0xa70fb20fbd0fc70f    0x7d0f860f910f9c0f
0x5555556b8ec0:    0x000000000000730f    0x0000000000000000
0x5555556b8ed0:    0x0000000000000000    0x0000000000000000
0x5555556b8ee0:    0x0000000000000000    0x0000000000000000

.    .    .

Unallocated space (all null bytes)

.    .    .

0x5555556b9dd0:    0x0000000000000000    0x0000000000000000
0x5555556b9de0:    0x0000000000000000    0x0000000000000000
0x5555556b9df0:    0x0000000000000000    0x0000000000000000
0x5555556b9e00:    0x1f03030d00000000    0x3434343434343401
0x5555556b9e10:    0x0213030d086f3434    0x030c07de00323232
0x5555556b9e20:    0x0b0965706f680815    0x0a34383632021503
0x5555556b9e30:    0x36320215030a097c    0x150309097c0a3438
0x5555556b9e40:    0x097c0a3438363202    0x3438363202150308
0x5555556b9e50:    0x3202150307097c0a    0x0306087c0a343836
0x5555556b9e60:    0x082b023535350213    0x0235353502130305
0x5555556b9e70:    0x1d03040c0304082b    0x3237353339353101
0x5555556b9e80:    0x3202130303080538    0x13030208de003232
0x5555556b9e90:    0x0108de0032323202    0xde00323232021303
```

## Database File

So sqlite3 will store databases in a single file, which we will call the database file. Now looking at this file, something becomes apparent. It is effectively just the memory pages used to model the database in memory. Here I have the `y` database file included.

A few things about it. First we can see that the size of the file is `0x2000`. The size of a page from what I've seen is `0x1000`, so we can see that we have two pages here.

We can see that the first page is the root page. The file header begins at offset `0x00`. The page header for the first page begins at offset `0x64`. The cell ptr area for the first page begins at offset `0x6c`. The cell content area of the first page begins at offset `0x0fca`. The page header of the second page begins at offset `0x1000`. The cell ptr area for the second page begins at offset `0x1008`. The cell content area for the second page begins at offset `0x1f64`.

Now when we run the binary, and run a select query to ensure the database has been loaded into memory, we can find the pages in memory (I just did it by searching for somewhat-unique values from the database file). What we will find, is that there are exact copies of the database file pages in memory. However from what I've seen, the pages are not adjacent in memory, but all of the data in the pages itself is the same as the database file (and same offsets from the start of the page).

## Page Space Allocation

So allocation of space on a page happens in the `allocateSpace` function. There are three spots from which memory on a page can be allocated. They are these three, and will be attempted in this order:

```
0.)   From the freelist
1.)   From defragmentation (then proceeds to 2)
2.)   From the unallocated space
```

One more thing. There are two things commonly referred to here, which are the `gap` and the `top`. The `top` refers to the first byte of the cell content area. The gap refers to the first byte directly after the cell ptrs area (so the first byte of the cell content area).  

#### FreeList Allocation

So this is the first memory allocation attempted. This will be attempted if the `offsetFree` value of the page header is not null, and `gap+2<=top` is true (gap is more than 1 bytes above the top). It will attempt to find a free spot that is the correct size with the `pageFindSlot` function. It will search for a free page with the `pageFindSlot` function.

The `pageFindSlot` function will search for a free page. It will start with the first free list, which the offset is specified at offset `0x01` from the page header. For the free list to be valid, each node in the list must be after the preceding (checks occur for that). In addition to that, there is a check to ensure the free list does not extend past the end of the page.

#### Page Defragmentation

This will be attempted if the `gap+2+nByte>top` (new allocated space will go past the top). Defragmentation is the process of aligning the cells of the page, as to merge fragments together into usable blocks of freed memory. The memory will then be allocated from the `Unallocated Space` region, with the new freed up memory.

Page defragmentation happens with the `defragmentPage` function. It primarily just copies the entries to the bottom of the page to defragment the page. There are two defined methods for defragmentation, a general case and an optimized case for when there are two nodes in the free list. For defragmentation, there are several checks that happen. While it does, it has two copies of the memory page. One that acts as a source, and one that acts as a destination, each with their own index. The index for the destination is referred to as `cbrk`, and the index for the source is `pc`. The `pc` index must stay within the confines of the cell content area for the source, and `cbrk` cannot go above (be lesser than) the first cell offset. For the checks, it checks that the data that is being written do not extend past the end of the page.

#### Unallocated Space

So this will be the last resort. This just allocates the space from the unallocated region.



