# Tokyocabinet

So this is just some documentation regarding the Tokyocabinet (https://fallabs.com/tokyocabinet/)) version `1.4.48`, which at this time is the latest version (03/12/2022).

Now, Tokyocabinet is a databasing API, you call from code you write. There are several different types of databases Tokyocabinet supports. I primarily focused on the hash database type. Below you will find docs, that either are related to the functionallity of that database, or just exploit POCs. Currently I have POCs for how to cause the actual database queries/insertions as it's running to leak/overwrite database records, using malicously crafted database records edited into the database files.

[Hash Database Format](hashdb.md)
[Hash Database Operations](hashdb_operations.md)
[Record Leaking](pocs/record_leaking/readme.md)
[Record Overwrite](pocs/record_overwriting/readme.md)
