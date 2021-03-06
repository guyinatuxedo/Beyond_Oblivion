# Beyond_Oblivion

So many database implementations will have the option to save the data in the database to a file, which will then be loaded when the database process restarts. This is so the data will persist when the database process stops. Now in many database implementations, if you can write to this file, you can cause certain database operations like queries, to do other things, like leak additional data, or overwrite other records. The purpose of this project is to look at several different database implementations, investigate what can be done if you can write to the database save file, and general figuring out how these database implementations work.

### Databases Looked At

Here are the different database implementations I looked:
* [dragonfly](dragonfly/readme.md)
* [icecoal](icecoal/readme.md)
* [pogreb](pogreb/readme.md)
* [sdb](sdb/readme.md)
* [sqlite3](sqlite3/readme.md)
* [tokyo-cabinet](tokyo-cabinet/readme.md)
* [whitedb](whitedb/readme.md)

### Talks

Some recorded talks I did for this project:
* [All of Them](https://www.youtube.com/watch?v=KEqI4qwgXw4&list=PLi6Qsk-pIooLk1jeWzvfzc7Obiu99hN1L)

### CVEs

Here is a list of CVEs that came from this project:
```
CVE-2021-45346
CVE-2022-27159
CVE-2022-29724
```
