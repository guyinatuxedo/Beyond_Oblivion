## Compiling

So a quick guide on compiling code to use Whitdb's api. So for the header, have this line:

```
#include "../Db/dbapi.h"
```

And then, compile it like this:

```
$	cc -O2 -I.. -o tut4  tut4.c ../whitedb.c -lm
```
