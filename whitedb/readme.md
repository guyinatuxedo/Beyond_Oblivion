## WhiteDB Record Leaking

So this is some documentation regarding to to leak data from WhiteDB. This is done against version `0.8-alpha`, which at the time of this, I belive is the latest version (`04/20/2022`):

From `config-gcc.h`:

```
/* Define to the full name and version of this package. */
#define PACKAGE_STRING "WhiteDB 0.8-alpha"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "whitedb"

/* Define to the version of this package. */
#define PACKAGE_VERSION "0.8-alpha"
```

Now, here are some documentation regarding the operations. The majority of the actual database operation isn't covered in these, but it covers a bit:

[Value Storage](value_storage.md)

[Database Operations](database_operations.md)

[Compiling](compiling.md)

[Record Leaking Example](record_leaking/readme.md)
