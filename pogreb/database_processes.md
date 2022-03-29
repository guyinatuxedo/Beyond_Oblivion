## Database Opening

So the process of openign up a database is done via the `Open` function from the `db` class (`db.go` file).

```
// Open opens or creates a new DB.
// The DB must be closed after use, by calling Close method.
func Open(path string, opts *Options) (*DB, error) {
	opts = opts.copyWithDefaults(path)


	if err := os.MkdirAll(path, 0755); err != nil {
		return nil, err
	}

	// Try to acquire a file lock.
	lock, acquiredExistingLock, err := createLockFile(opts)
	if err != nil {
		if err == os.ErrExist {
			err = errLocked
		}
		return nil, errors.Wrap(err, "creating lock file")
	}
	clean := lock.Unlock
	defer func() {
		if clean != nil {
			_ = clean()
		}
	}()

	if acquiredExistingLock {
		// Lock file already existed, but the process managed to acquire it.
		// It means the database wasn't closed properly.
		// Start recovery process.
		if err := backupNonsegmentFiles(opts.FileSystem); err != nil {
			return nil, err
		}
	}

	index, err := openIndex(opts)
	if err != nil {
		return nil, errors.Wrap(err, "opening index")
	}

	datalog, err := openDatalog(opts)
	if err != nil {
		return nil, errors.Wrap(err, "opening datalog")
	}

	db := &DB{
		opts:       opts,
		index:      index,
		datalog:    datalog,
		lock:       lock,
		metrics:    &Metrics{},
		syncWrites: opts.BackgroundSyncInterval == -1,
	}
	if index.count() == 0 {
		// The index is empty, make a new hash seed.
		seed, err := hash.RandSeed()
		if err != nil {
			return nil, err
		}
		db.hashSeed = seed
	} else {
		if err := db.readMeta(); err != nil {
			return nil, errors.Wrap(err, "reading db meta")
		}
	}

	if acquiredExistingLock {
		if err := db.recover(); err != nil {
			return nil, errors.Wrap(err, "recovering")
		}
	}

	if db.opts.BackgroundSyncInterval > 0 || db.opts.BackgroundCompactionInterval > 0 {
		db.startBackgroundWorker()
	}

	clean = nil

	return db, nil
}
```

So starting off, it will create the directories (if it doesn't exist), and get lock files for all of the files. Then it will attempt the index and the datalog with the `openIndex` and `openDatalog`functions. With the `openIndex` function from `index.go`, we see this:

```
func openIndex(opts *Options) (*index, error) {
	main, err := openFile(opts.FileSystem, indexMainName, false)
	if err != nil {
		return nil, errors.Wrap(err, "opening main index")
	}
	overflow, err := openFile(opts.FileSystem, indexOverflowName, false)
	if err != nil {
		_ = main.Close()
		return nil, errors.Wrap(err, "opening overflow index")
	}
	idx := &index{
		opts:       opts,
		main:       main,
		overflow:   overflow,
		numBuckets: 1,
	}
	if main.empty() {
		// Add an empty bucket.
		if _, err = idx.main.extend(bucketSize); err != nil {
			_ = main.Close()
			_ = overflow.Close()
			return nil, err
		}
	} else if err := idx.readMeta(); err != nil {
		_ = main.Close()
		_ = overflow.Close()
		return nil, errors.Wrap(err, "opening index meta")
	}
	return idx, nil
}
```

We see here, it will attempt to read the `main` and `overflow` index files. If there are no buckets, it will make one via the `extend` function. Next up in the `openDatalog` function, we see that it will effectively just open up the various memory segments via reading in the segment files:

```
func openDatalog(opts *Options) (*datalog, error) {
	files, err := opts.FileSystem.ReadDir(".")
	if err != nil {
		return nil, err
	}

	dl := &datalog{
		opts: opts,
	}

	// Open existing segments.
	for _, file := range files {
		name := file.Name()
		ext := filepath.Ext(name)
		if ext != segmentExt {
			continue
		}
		id, seqID, err := parseSegmentName(name)
		if err != nil {
			return nil, err
		}
		seg, err := dl.openSegment(name, id, seqID)
		if err != nil {
			return nil, errors.Wrapf(err, "opening segment %s", name)
		}
		if seg.sequenceID > dl.maxSequenceID {
			dl.maxSequenceID = seg.sequenceID
		}
		dl.segments[seg.id] = seg
	}

	if err := dl.swapSegment(); err != nil {
		return nil, err
	}

	return dl, nil
}
```

Proceeding that, in the `Open` function, it will generate a new hash seed if it is creating a new database file. If the database was already created, it will read in the database metadata.

## Database Selection

So this covers the process of how this database will retrieve a record from a database. This is done with the `Get` function from the `db` class:

```
// Get returns the value for the given key stored in the DB or nil if the key doesn't exist.
func (db *DB) Get(key []byte) ([]byte, error) {
	h := db.hash(key)
	db.metrics.Gets.Add(1)
	db.mu.RLock()
	defer db.mu.RUnlock()
	var retValue []byte
	err := db.index.get(h, func(sl slot) (bool, error) {
		if uint16(len(key)) != sl.keySize {
			return false, nil
		}
		slKey, value, err := db.datalog.readKeyValue(sl)
		if err != nil {
			return true, err
		}
		if bytes.Equal(key, slKey) {
			retValue = cloneBytes(value)
			return true, nil
		}
		db.metrics.HashCollisions.Add(1)
		return false, nil
	})
	if err != nil {
		return nil, err
	}
	return retValue, nil
}
```

So starting off, it will hash the key (this is covered in the hashing section). Next it will attempt to get the bucket associated with the key. This is done with the `get` function from the `index` class (`get` from `index.go`) function. We also see that as part of the call to `get`, it will pass in the actual comparator function used to evaluate the keys:

```
func (idx *index) get(hash uint32, matchKey matchKeyFunc) error {
	it := idx.newBucketIterator(idx.bucketIndex(hash))
	for {
		b, err := it.next()
		if err == ErrIterationDone {
			return nil
		}
		if err != nil {
			return err
		}
		for i := 0; i < slotsPerBucket; i++ {
			sl := b.slots[i]
			// No more slots in the bucket.
			if sl.offset == 0 {
				break
			}
			if hash != sl.hash {
				continue
			}
			if match, err := matchKey(sl); match || err != nil {
				return err
			}
		}
	}
}
```

Now this function will do two things. It will retrieve the index to the bucket, which should contain the record. Then it will iterate through the bucket, looking for the record. Now to get the bucket index, this is done with the `bucketIndex` function within the `index` class. With some printf statements, we see that for a small database the `idx` is `0`:

```
func (idx *index) bucketIndex(hash uint32) uint32 {
	bidx := hash & ((1 << idx.level) - 1)
	if bidx < idx.splitBucketIdx {
		return hash & ((1 << (idx.level + 1)) - 1)
	}
	return bidx
}
```

Next up, it will create a `newBucketIterator` using the bucket index, which contains the bucket offset. Also `headerSize` and `bucketSize` are both `512` bytes:

```
type bucketIterator struct {
	off      int64 // Offset of the next bucket.
	f        *file // Current index file.
	overflow *file // Overflow index file.
}

// bucketOffset returns on-disk bucket offset by the bucket index.
func bucketOffset(idx uint32) int64 {
	return int64(headerSize) + (int64(bucketSize) * int64(idx))
}

func (idx *index) newBucketIterator(startBucketIdx uint32) *bucketIterator {
	return &bucketIterator{
		off:      bucketOffset(startBucketIdx),
		f:        idx.main,
		overflow: idx.overflow,
	}
}
```

Proceeding that, we see it will just iterate through the bucket, checking if the record is the one it is looking for. This is primarily done in the function that is passed in:

```
func(sl slot) (bool, error) {
		if uint16(len(key)) != sl.keySize {
			return false, nil
		}
		slKey, value, err := db.datalog.readKeyValue(sl)
		if err != nil {
			return true, err
		}
		if bytes.Equal(key, slKey) {
			retValue = cloneBytes(value)
			return true, nil
		}
		db.metrics.HashCollisions.Add(1)
		return false, nil
	})
```

So we can see, it will attempt to read the record using the `readKeyValue` function from the `datalog` class:

```
func (dl *datalog) readKeyValue(sl slot) ([]byte, []byte, error) {
	off := int64(sl.offset) + 6 // Skip key size and value size.
	seg := dl.segments[sl.segmentID]
	keyValue, err := seg.Slice(off, off+int64(sl.kvSize()))
	if err != nil {
		return nil, nil, err
	}
	return keyValue[:sl.keySize], keyValue[sl.keySize:], nil
}
```

Looking at this function, we see it looks like it wraps the `Slice` function from the `mem` class, which just returns the memory between the two offsets that are given. We see it just uses the size value listed from the bucket:

```
func (f *memFile) Slice(start int64, end int64) ([]byte, error) {
	if f.closed {
		return nil, os.ErrClosed
	}
	if end > f.size {
		return nil, io.EOF
	}
	return f.buf[start:end], nil
}
```

So proceeding that, we see that in the function passed in as an argument to the original `db.index.get` function, if the keys are equivalent, it will clone the record and return it. Looking at the original `Get` call, that appears to be the end of it.

## Database Insertion

So this starts off with the `Put` function from the `db` class:

```
// Put sets the value for the given key. It updates the value for the existing key.
func (db *DB) Put(key []byte, value []byte) error {
	if len(key) > MaxKeyLength {
		return errKeyTooLarge
	}
	if len(value) > MaxValueLength {
		return errValueTooLarge
	}
	h := db.hash(key)
	db.metrics.Puts.Add(1)
	db.mu.Lock()
	defer db.mu.Unlock()

	segID, offset, err := db.datalog.put(key, value)
	if err != nil {
		return err
	}

	sl := slot{
		hash:      h,
		segmentID: segID,
		keySize:   uint16(len(key)),
		valueSize: uint32(len(value)),
		offset:    offset,
	}

	if err := db.put(sl, key); err != nil {
		return err
	}

	if db.syncWrites {
		return db.sync()
	}
	return nil
}
```

So starting off, it will hash the key, the same way for record retrieval using the `hash` function. Nex tup, it will actually add the record to the datalog using the `put` function from the `datalog` class:

```
func (dl *datalog) put(key []byte, value []byte) (uint16, uint32, error) {
	return dl.writeRecord(encodePutRecord(key, value), recordTypePut)
}
```

Which just wraps the `writeRecord` and `encodePutRecord` functions. Looking at the `encodePutRecord` function (from `segment.go`) we see this:

```
func encodedRecordSize(kvSize uint32) uint32 {
	// key size, value size, key, value, crc32
	return 2 + 4 + kvSize + 4
}

func encodeRecord(key []byte, value []byte, rt recordType) []byte {
	size := encodedRecordSize(uint32(len(key) + len(value)))
	data := make([]byte, size)
	binary.LittleEndian.PutUint16(data[:2], uint16(len(key)))

	valLen := uint32(len(value))
	if rt == recordTypeDelete { // Set delete bit.
		valLen |= 1 << 31
	}
	binary.LittleEndian.PutUint32(data[2:], valLen)

	copy(data[6:], key)
	copy(data[6+len(key):], value)
	checksum := crc32.ChecksumIEEE(data[:6+len(key)+len(value)])
	binary.LittleEndian.PutUint32(data[size-4:size], checksum)
	return data
}

func encodePutRecord(key []byte, value []byte) []byte {
	return encodeRecord(key, value, recordTypePut)
}
```

So we can see that in order to encode the record, it first stores the size of the key (2 byte int), then the size of the value (2 byte int), then appears to be two bytes of just blank space. After that it will store the key, and immediately following it the value. Proceeding that it will store a `4` byte `crc32` checksum for the record. The generated bytes here are what actually end up modeling the record in memory.

Now moving onto the `writeRecord` function from the `datalog` file, we see this:

```
func (dl *datalog) writeRecord(data []byte, rt recordType) (uint16, uint32, error) {
	if dl.curSeg.meta.Full || dl.curSeg.size+int64(len(data)) > int64(dl.opts.maxSegmentSize) {
		// Current segment is full, create a new one.
		dl.curSeg.meta.Full = true
		if err := dl.swapSegment(); err != nil {
			return 0, 0, err
		}
	}
	off, err := dl.curSeg.append(data)
	if err != nil {
		return 0, 0, err
	}
	switch rt {
	case recordTypePut:
		dl.curSeg.meta.PutRecords++
	case recordTypeDelete:
		dl.curSeg.meta.DeleteRecords++
	}
	return dl.curSeg.id, uint32(off), nil
}
```

So it does some checking, but we can see it uses the `append` function from `file.go` to actually append the record, which we see it just appends it to the end of the region. After that, it will increment either the `PutRecords/DeleteRecords` count, so this code path is probably also used for deleting records as well:

```
func (f *file) append(data []byte) (int64, error) {
	off := f.size
	if _, err := f.WriteAt(data, off); err != nil {
		return 0, err
	}
	f.size += int64(len(data))
	return off, nil
}
```

So following the `put` from the datalog, it will call the `put` (smaller `p`) from the database class. Before that, we see it actually constructs the slot, which is the thing that is actually getting inserted. We see that it's structure is this:
```
	sl := slot{
		hash:      h,
		segmentID: segID,
		keySize:   uint16(len(key)),
		valueSize: uint32(len(value)),
		offset:    offset,
	}
```

Now moving on:

```
func (db *DB) put(sl slot, key []byte) error {
	return db.index.put(sl, func(cursl slot) (bool, error) {
		if uint16(len(key)) != cursl.keySize {
			return false, nil
		}
		slKey, err := db.datalog.readKey(cursl)
		if err != nil {
			return true, err
		}
		if bytes.Equal(key, slKey) {
			db.datalog.trackDel(cursl) // Overwriting existing key.
			return true, nil
		}
		return false, nil
	})
}
```

This function effectively just wraps the `put` function from the `index` class. Except it will also specify the function for checking the key:

```
func(cursl slot) (bool, error) {
		if uint16(len(key)) != cursl.keySize {
			return false, nil
		}
		slKey, err := db.datalog.readKey(cursl)
		if err != nil {
			return true, err
		}
		if bytes.Equal(key, slKey) {
			db.datalog.trackDel(cursl) // Overwriting existing key.
			return true, nil
		}
		return false, nil
	}
```

We see that if it finds the same key (which means that a record exists at the key it's trying to insert into) it will delete the record using the `datalog.trackDel` function. But looking at the `index.put` function, we see this:

```
func (idx *index) put(newSlot slot, matchKey matchKeyFunc) error {
	if idx.numKeys == MaxKeys {
		return errFull
	}
	sw, overwritingExisting, err := idx.findInsertionBucket(newSlot, matchKey)
	if err != nil {
		return err
	}
	if err := sw.insert(newSlot, idx); err != nil {
		return err
	}
	if err := sw.write(); err != nil {
		return err
	}
	if overwritingExisting {
		return nil
	}
	idx.numKeys++
	if float64(idx.numKeys)/float64(idx.numBuckets*slotsPerBucket) > loadFactor {
		if err := idx.split(); err != nil {
			return err
		}
	}
	return nil
}
```

So we can see it starts off with getting the actual bucket to insert into (`sw`), with this function:

```
func (idx *index) findInsertionBucket(newSlot slot, matchKey matchKeyFunc) (*slotWriter, bool, error) {
	sw := &slotWriter{}
	it := idx.newBucketIterator(idx.bucketIndex(newSlot.hash))
	for {
		b, err := it.next()
		if err == ErrIterationDone {
			return nil, false, errors.New("failed to insert a new slot")
		}
		if err != nil {
			return nil, false, err
		}
		sw.bucket = &b
		var i int
		for i = 0; i < slotsPerBucket; i++ {
			sl := b.slots[i]
			if sl.offset == 0 {
				// Found an empty slot.
				sw.slotIdx = i
				return sw, false, nil
			}
			if newSlot.hash != sl.hash {
				continue
			}
			match, err := matchKey(sl)
			if err != nil {
				return nil, false, err
			}
			if match {
				// Key already in the index.
				// The slot writer will overwrite the existing slot.
				sw.slotIdx = i
				return sw, true, nil
			}
		}
		if b.next == 0 {
			// No more buckets in the chain.
			sw.slotIdx = i
			return sw, false, nil
		}
	}
}
```

We see that the process is pretty similar to that of the record retrieval process.

Next up we have the `insert/write` functions from `bucket.go` file, which looking at it, looks like it pretty much just writes it to the file:

```
func (b *bucketHandle) write() error {
	buf, err := b.MarshalBinary()
	if err != nil {
		return err
	}
	_, err = b.file.WriteAt(buf, b.offset)
	return err
}

// slotWriter inserts and writes slots into a bucket.
type slotWriter struct {
	bucket      *bucketHandle
	slotIdx     int
	prevBuckets []*bucketHandle
}

func (sw *slotWriter) insert(sl slot, idx *index) error {
	if sw.slotIdx == slotsPerBucket {
		// Bucket is full, create a new overflow bucket.
		nextBucket, err := idx.createOverflowBucket()
		if err != nil {
			return err
		}
		sw.bucket.next = nextBucket.offset
		sw.prevBuckets = append(sw.prevBuckets, sw.bucket)
		sw.bucket = nextBucket
		sw.slotIdx = 0
	}
	sw.bucket.slots[sw.slotIdx] = sl
	sw.slotIdx++
	return nil
}

func (sw *slotWriter) write() error {
	// Write previous buckets first.
	for i := len(sw.prevBuckets) - 1; i >= 0; i-- {
		if err := sw.prevBuckets[i].write(); err != nil {
			return err
		}
	}
	return sw.bucket.write()
}
```

So in conclusion, it first isnerts the record into the datalog, than into the index.

## Database Close

So this process starts off with the `Close` function from the `db` class:

```
// Close closes the DB.
func (db *DB) Close() error {
	if db.cancelBgWorker != nil {
		db.cancelBgWorker()
	}
	db.closeWg.Wait()
	db.mu.Lock()
	defer db.mu.Unlock()
	if err := db.writeMeta(); err != nil {
		return err
	}
	if err := db.datalog.close(); err != nil {
		return err
	}
	if err := db.index.close(); err != nil {
		return err
	}
	if err := db.lock.Unlock(); err != nil {
		return err
	}
	return nil
}
```

So starting off, we see it will call the `writeMeta` function from the database class:

```
func (db *DB) writeMeta() error {
	m := dbMeta{
		HashSeed: db.hashSeed,
	}
	return writeGobFile(db.opts.FileSystem, dbMetaName, m)
}
```

We see, it effectively just dumps the database hashseed to the dbMeta file, which via inserting a print statemenet, we see is the `db.pmt` file. Also we see it calls the `writeGobFile` function, which is from the `gobfile.go` file:

```
func writeGobFile(fsys fs.FileSystem, name string, v interface{}) error {
	f, err := openFile(fsys, name, true)
	if err != nil {
		return err
	}
	defer f.Close()
	enc := gob.NewEncoder(f)
	return enc.Encode(v)
}
```

Effectively it just uses the `gob` go standard library to encode the data, then writes it directly to a file.

Next up, we have the `db.datalog.close()` call:

```
func (dl *datalog) close() error {
	for _, seg := range dl.segments {
		if seg == nil {
			continue
		}
		if err := seg.Close(); err != nil {
			return err
		}
		metaName := seg.name + metaExt
		if err := writeGobFile(dl.opts.FileSystem, metaName, seg.meta); err != nil {
			return err
		}
	}
	return nil
}
```

So we see, it effectively just dumps the data from the datalog (which actuall holds the records) to a file. It does this via iterating through all of the segments, closing them out, then dumping there data to a file. Which with a print statement, we see it's `00000-1.psg` and `00000-1.psg.pmt`. If we had more segments (which we would need a lot more records for), we would probably see mulitple files with that extension, due to the multiple segments. 

Then next up, there is the `db.index.close()` function:

```
func (idx *index) close() error {
	if err := idx.writeMeta(); err != nil {
		return err
	}
	if err := idx.main.Close(); err != nil {
		return err
	}
	if err := idx.overflow.Close(); err != nil {
		return err
	}
	return nil
}
```

So we wsee it will call the `idx.writeMeta` function, which will dump the metadata to the `index.pmt` file:

```
func (idx *index) writeMeta() error {
	m := indexMeta{
		Level:               idx.level,
		NumKeys:             idx.numKeys,
		NumBuckets:          idx.numBuckets,
		SplitBucketIndex:    idx.splitBucketIdx,
		FreeOverflowBuckets: idx.freeBucketOffs,
	}
	return writeGobFile(idx.opts.FileSystem, indexMetaName, m)
}
```

Dollowing that, the `index.close()` function will just close the `main` and `overflow` files (correspond to the `mian.pix` and `overflow.pix` files).

So effectively, the process of closing the database is just the database writing it's data to files, and closing everything.



## Hashing

So this part will cover hashing. This database operates off of a hashing design. When it attempt to retrieve or store a record, it will hash the key in order to determine it's position. For the hashing algorithm, there are two inputs. The first is the key itself, the second is a special seed that is specific to the database.

For hashing, it will use the `hash` function from the `db` class:

```
func (db *DB) hash(data []byte) uint32 {
	return hash.Sum32WithSeed(data, db.hashSeed)
}
```

Which we can see, is effectively just a wrapper for the `hash.Sum32WithSeed`, which it will call with the input data, and the database hashseed:

```
// Sum32WithSeed is a port of MurmurHash3_x86_32 function.
func Sum32WithSeed(data []byte, seed uint32) uint32 {
	h1 := seed
	dlen := len(data)

	for len(data) >= 4 {
		k1 := uint32(data[0]) | uint32(data[1])<<8 | uint32(data[2])<<16 | uint32(data[3])<<24
		data = data[4:]

		k1 *= c1
		k1 = bits.RotateLeft32(k1, 15)
		k1 *= c2

		h1 ^= k1
		h1 = bits.RotateLeft32(h1, 13)
		h1 = h1*5 + 0xe6546b64
	}

	var k1 uint32
	switch len(data) {
	case 3:
		k1 ^= uint32(data[2]) << 16
		fallthrough
	case 2:
		k1 ^= uint32(data[1]) << 8
		fallthrough
	case 1:
		k1 ^= uint32(data[0])
		k1 *= c1
		k1 = bits.RotateLeft32(k1, 15)
		k1 *= c2
		h1 ^= k1
	}

	h1 ^= uint32(dlen)

	h1 ^= h1 >> 16
	h1 *= 0x85ebca6b
	h1 ^= h1 >> 13
	h1 *= 0xc2b2ae35
	h1 ^= h1 >> 16

	return h1
}
```

I'm not going to reverse the hashing algorithm. Instead here is a simple go program I basically copied the src, that way I can directly see what hash value will be generated from certain inputs:

```
package main

import (
	"math/bits"
	"log"
    "fmt"
)

const (
    c1 uint32 = 0xcc9e2d51
    c2 uint32 = 0x1b873593
)

// Sum32WithSeed is a port of MurmurHash3_x86_32 function.
func Sum32WithSeed(data []byte, seed uint32) uint32 {
    h1 := seed
    dlen := len(data)

    for len(data) >= 4 {
        k1 := uint32(data[0]) | uint32(data[1])<<8 | uint32(data[2])<<16 | uint32(data[3])<<24
        data = data[4:]

        k1 *= c1
        k1 = bits.RotateLeft32(k1, 15)
        k1 *= c2

        h1 ^= k1
        h1 = bits.RotateLeft32(h1, 13)
        h1 = h1*5 + 0xe6546b64
    }

    var k1 uint32
    switch len(data) {
    case 3:
        k1 ^= uint32(data[2]) << 16
        fallthrough
    case 2:
        k1 ^= uint32(data[1]) << 8
        fallthrough
    case 1:
        k1 ^= uint32(data[0])
        k1 *= c1
        k1 = bits.RotateLeft32(k1, 15)
        k1 *= c2
        h1 ^= k1
    }

    h1 ^= uint32(dlen)

    h1 ^= h1 >> 16
    h1 *= 0x85ebca6b
    h1 ^= h1 >> 13
    h1 *= 0xc2b2ae35
    h1 ^= h1 >> 16

    return h1
}


func main() {
    var hashSeed uint32
    var data []byte

    // Declare Inputs
    data = []byte("vegeta")
    hashSeed = 0x88cefebe

    // generate the hash
    hashValue := Sum32WithSeed(data, hashSeed)

    // Print the values
    log.Printf("Input Key: %s", string(data))
    log.Printf(fmt.Sprintf("Input Seed: 0x%x", hashSeed))
    log.Printf(fmt.Sprintf("0x%x", hashValue))
}
```



