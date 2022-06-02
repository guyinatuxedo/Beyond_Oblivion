# Icecoal

So this is a fairly simple database implementation from `https://github.com/ppml38/icecoal`. It is effectively just a CSV parser/editor, where it stores tables as a CSV.

## Database Operations

For more information on how the database operations work:

[Database Operations](database_operations.md)

## Attack

So full disclosure, not too sure if this can be called an attack. But there is a way you can query one table, and query another. This is done via simply making a symbolic link. Since tabels are just CSV files, and there are no real checks to stop something like this. You can create a symbolic link to one table with a different name. Since table files should have the same name, you can just name the symbolic link something else, than query that to query the original. Time for a demo.

