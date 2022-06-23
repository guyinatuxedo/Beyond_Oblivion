import icecoal

# Create the database
icecoal.query("create database test")

# Create the table table0
icecoal.query("create table test/table0(val0, val1)")

# Insert 3 records into table0
icecoal.query("insert into test/table0('x0', 'y0')")
icecoal.query("insert into test/table0('x1', 'y1')")
icecoal.query("insert into test/table0('x2', 'y2')")

