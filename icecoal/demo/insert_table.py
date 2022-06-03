import icecoal

# View all contents of table1
print(icecoal.query("select * from test/table1"))

# Insert 3 records into table1
icecoal.query("insert into test/table1('a0', 'b0')")
icecoal.query("insert into test/table1('a1', 'b1')")
icecoal.query("insert into test/table1('a2', 'b2')")

# View all contents of table1
print(icecoal.query("select * from test/table1"))

