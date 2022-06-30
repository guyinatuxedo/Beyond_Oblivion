# SDB

So I wasn't able to really do amything fun with this database implementation. See below for some details as to how this database implementation works. This is probably the least documented DB implementation I have in here.

# Hashing

So one thing this database implementation will do, is it will hash the keys as a string. It does so using the `sdb_hash_len` function which we see here (just some binary shifting, xoring, and adding the numeric value of the ascii string):

```
SDB_API ut32 sdb_hash_len(const char *s, ut32 *len) {
	ut32 h = CDB_HASHSTART;
#if FORCE_COLLISION
	h = 0;
	while (*s) {
		h += *s;
		s++;
	}
#else
	ut32 count = 0;
	if (s) {
		while (*s) {
			h = (h + (h << 5)) ^ *s++;
			count++;
		}
	}
	if (len) {
		*len = count;
	}
#endif
	return h;
}

SDB_API ut32 sdb_hash(const char *s) {
	return sdb_hash_len (s, NULL);
}
```

Which we see, the constant `CDB_HASHTART` is equal to `0x1501` (`5381`):

```
./sdb/src/cdb.h:#define CDB_HASHSTART 5381
```

So the string hashing algorithm translates to this following python3 code:

```
def hash_sdb(key):
	x = 0x1505
	for i in key:
		x = ((x + (x << 5)) ^ ord(i)) & 0xffffffff
	print(hex(x))
```

# Selection

So this will document the code path for selection. This starts off with the `sdb_get` function:

```
SDB_API char *sdb_get(Sdb* s, const char *key, ut32 *cas) {
	return sdb_get_len (s, key, NULL, cas);
}
```

Which wraps the `sdb_get_len` function:

```
SDB_API char *sdb_get_len(Sdb* s, const char *key, int *vlen, ut32 *cas) {
	const char *value = sdb_const_get_len (s, key, vlen, cas);
	return value ? strdup (value) : NULL;
}
```

Which wraps the `sdb_const_get_len` function:

```
SDB_API const char *sdb_const_get_len(Sdb* s, const char *key, int *vlen, ut32 *cas) {
	ut32 pos, len;
	ut64 now = 0LL;
	bool found;

	if (cas) {
		*cas = 0;
	}
	if (vlen) {
		*vlen = 0;
	}
	if (!s || !key) {
		return NULL;
	}
	// TODO: optimize, iterate once
	size_t keylen = strlen (key);

	/* search in memory */
	if (s->ht) {
		SdbKv *kv = (SdbKv*) sdb_ht_find_kvp (s->ht, key, &found);
		if (found) {
			if (!sdbkv_value (kv) || !*sdbkv_value (kv)) {
				return NULL;
			}
			if (s->timestamped && kv->expire) {
				if (!now) {
					now = sdb_now ();
				}
				if (now > kv->expire) {
					sdb_unset (s, key, 0);
					return NULL;
				}
			}
			if (cas) {
				*cas = kv->cas;
			}
			if (vlen) {
				*vlen = sdbkv_value_len (kv);
			}
			return sdbkv_value (kv);
		}
	}
	/* search in gperf */
	if (s->gp && s->gp->get) {
		return s->gp->get (key);
	}
	/* search in disk */
	if (s->fd == -1) {
		return NULL;
	}
	(void) cdb_findstart (&s->db);
	if (!s->ht || cdb_findnext (&s->db, s->ht->opt.hashfn (key), key, keylen) < 1) {
		return NULL;
	}
	len = cdb_datalen (&s->db);
	if (len < SDB_MIN_VALUE || len >= SDB_MAX_VALUE) {
		return NULL;
	}
	if (vlen) {
		*vlen = len;
	}
	pos = cdb_datapos (&s->db);
	return s->db.map + pos;
}
```

Which calls the `cdb_findnext` function, which is where the primary selection logic actually takes place:

```
int cdb_findnext(struct cdb *c, ut32 u, const char *key, ut32 len) {
	char buf[8];
	ut32 pos;
	int m;
	len++;
	if (c->fd == -1) {
		return -1;
	}
	c->hslots = 0;
	if (!c->loop) {
		const int bufsz = ((u + 1) & 0xFF) ? sizeof (buf) : sizeof (buf) / 2;
		if (!cdb_read (c, buf, bufsz, (u << 2) & 1023)) {
			return -1;
		}
		/* hslots = (hpos_next - hpos) / 8 */
		ut32_unpack (buf, &c->hpos);
		if (bufsz == sizeof (buf)) {
			ut32_unpack (buf + 4, &pos);
		} else {
			pos = c->size;
		}
		if (pos < c->hpos) {
			return -1;
		}
		c->hslots = (pos - c->hpos) / (2 * sizeof (ut32));
		if (!c->hslots) {
			return 0;
		}
		c->khash = u;
		u = ((u >> 8) % c->hslots) << 3;
		c->kpos = c->hpos + u;
	}
	while (c->loop < c->hslots) {
		if (!cdb_read (c, buf, sizeof (buf), c->kpos)) {
			return 0;
		}
		ut32_unpack (buf + 4, &pos);
		if (!pos) {
			return 0;
		}
		c->loop++;
		c->kpos += sizeof (buf);
		if (c->kpos == c->hpos + (c->hslots << 3)) {
			c->kpos = c->hpos;
		}
		ut32_unpack (buf, &u);
		if (u == c->khash) {
			if (!cdb_getkvlen (c, &u, &c->dlen, pos) || !u) {
				return -1;
			}
			if (u == len) {
				if ((m = match (c, key, len, pos + KVLSZ)) == -1) {
					return 0;
				}
				if (m == 1) {
					c->dpos = pos + KVLSZ + len;
					return 1;
				}
			}
		}
	}
	return 0;
}
```

So before we go through the selection logic, a bit about how this database works. It works off of a hashmap implementation. In order to find a record, it takes the key (as a string), and hashes it using the algorithm described. It will then use that hash, as an index into an array. The values it get from the array will lead it to a bucket. This bucket will store records (or information as to where the records are). That way when a hash collision occurs, it can still store both buckets.

So in here, the if then statement with the `if (!c->loop) {` conditional's purpose is to obtain the bucket. To get the index into the bucket array, it will calculate the index via `(u << 2) & 1023` where `u` is the hash of the key. The index it got will be used as the offset to the database file (in reality, the in memory model of the database, which correlates to the database file). Here will be `0x08` bytes of data, which consist of two `0x04` byte integers. These two integers are offsets, which represent the start and end of the bucket. To read the `0x08` bytes of data the `cdb_read` function is called, and `ut32_unpack` is called twice to unpack the two `0x04` byte offsets. The beginning of the bucket is stored in `c->hpos` (first `0x04` bytes), and the end of the bucket is stored in `pos` (second `0x04` bytes).

Following that, there is a `while` loop, that will iterate through the slots in the bucket. A slot contains two `0x04` byte integer values, the first being the actual hash, and the second being the offset to the record itself. Iterating through the slots in the bucket, it will first check if the hashes match. If they do, then it will call the `match` function, which will actually compare the stored key with the key we're querying for. If they match, it will then set the position of the `CDB` object to the position of the value stored in the record.

So the `cdb_findnext` function will return the offset within the database. The `sdb_const_get_len` function will take that offset, and actually get a ptr to the value in memory (add the database base to the offset to get the ptr to the value, with `return s->db.map + pos;`). Then in the `sdb_get_len` function it will call the `strdup` function on the value, and return the ptr. This means that the ptr to the data we get, will be cut off at the null byte.
