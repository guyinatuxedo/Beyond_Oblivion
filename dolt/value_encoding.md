# DB Database Creation

```
$	./dolt init
$	./dolt sql -q "create table within (meme varchar(30), dankness int, primary key (dankness))"
$	./dolt sql -q "insert into within (meme, dankness) values ('herding', 55), ('the', 65), ('weak', 75), ('towards', 85)"
$	./dolt sql -q "select * from within"
```

Compiling:
```
https://docs.dolthub.com/introduction/installation/source
$	ls ~/go/bin/
dolt  git-dolt  git-dolt-smudge  hope-dolt
```

Introspection:
```
	f, err := os.OpenFile("/tmp/nether", os.O_APPEND|os.O_CREATE|os.O_WRONLY, 0644)
	f.Write([]byte(fmt.Sprintf("\n\nWhat's Up?: %u\nString: %s", size, strBytes)))
    fmt.Print(err);
```


# Value Encoding

So this section here details how values are encoded. 

## Specific Value Encoding Types

```
0x00	-	Bool
0x01	-	Int
0x02	-	String
0x05	-	List
0x06	-	Map
0x07	-	Ref
0x08	-	Set
0x09	-	Struct
0x0F	-	Int
0x10	-	Uint
0x12	-	Tuple
```