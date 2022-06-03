# Icecoal

So this is a fairly simple database implementation from `https://github.com/ppml38/icecoal`. It is effectively just a CSV parser/editor, where it stores tables as a CSV. It works as a python3 api.

## Database Operations

For more information on how the database operations work:

[Database Operations](database_operations.md)

## Attack

So full disclosure, not too sure if this can be called an attack. But there is a way you can query one table, and query another. This is done via simply making a symbolic link. Since tabels are just CSV files, and there are no real checks to stop something like this. You can create a symbolic link to one table with a different name. Since table files should have the same name, you can just name the symbolic link something else, than query that to query the original. Time for a demo.

Since the database is a python3 api, here are the scripts used for the demo:

Create table0, insert some records (`create_table.py`):

```
import icecoal

# Create the database
icecoal.query("create database test")

# Create the table table0
icecoal.query("create table test/table0(val0, val1)")

# Insert 3 records into table0
icecoal.query("insert into test/table0('x0', 'y0')")
icecoal.query("insert into test/table0('x1', 'y1')")
icecoal.query("insert into test/table0('x2', 'y2')")
```

View table0 (`view_table.py`):

```
import icecoal

# View all of the contents of the database
print(icecoal.query("select * from test/table0"))

```

Select from a secondary table (table1), insert into the secondary table, and then select from it again (`insert_table.py`):

```
import icecoal

# View all contents of table1
print(icecoal.query("select * from test/table1"))

# Insert 3 records into table1
icecoal.query("insert into test/table1('a0', 'b0')")
icecoal.query("insert into test/table1('a1', 'b1')")
icecoal.query("insert into test/table1('a2', 'b2')")

# View all contents of table1
print(icecoal.query("select * from test/table1"))
```

So first let's go ahead and make the database, and `table0`:

```
$	ls
create_table.py  insert_table.py  view_table.py
$	python3 create_table.py 
$	ls
create_table.py  insert_table.py  test  view_table.py
$	ls
create_table.py  insert_table.py  test  view_table.py
$	cat test/table0 
val0,val1
x0,y0
x1,y1
x2,y2
```

Looking at the contents of `table0`, we see it consists of three seperate rows:

```
$	python3 view_table.py 
[0, '3 rows selected', [['x0', 'y0'], ['x1', 'y1'], ['x2', 'y2']]]
```

Now let's create a symbolic link to `table0`, to a seperate file called `table1`:

```
$	cd test/
$	ln -s table0 table1
$	cat table1
val0,val1
x0,y0
x1,y1
x2,y2
$	cd ../
```

Viewing the contents of `table1`, we see it's the same as `table0`. Let's insert some values into it:

```
$	python3 insert_table.py 
[0, '3 rows selected', [['x0', 'y0'], ['x1', 'y1'], ['x2', 'y2']]]
[0, '6 rows selected', [['x0', 'y0'], ['x1', 'y1'], ['x2', 'y2'], ['a0', 'b0'], ['a1', 'b1'], ['a2', 'b2']]]
```

And then looking at the contents of `table0` again, we see it was changed. So we've proven that using symbolic links, we can query one table, and actually query a seperate table:

```
$	python3 view_table.py 
[0, '6 rows selected', [['x0', 'y0'], ['x1', 'y1'], ['x2', 'y2'], ['a0', 'b0'], ['a1', 'b1'], ['a2', 'b2']]]
```