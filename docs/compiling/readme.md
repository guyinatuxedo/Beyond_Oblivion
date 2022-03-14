


```
$	gcc -I. -I.. -ansi -Wall -pedantic -fsigned-char -O2 -o tcfdbex tcfdbex.o  -L. -L.. -ltokyocabinet -lz -lbz2 -lpthread -lm -lc
```




```
$	make
gcc -c -I. -I.. -ansi -Wall -pedantic -fsigned-char -O2 tcutilex.c
LD_RUN_PATH=/lib:/usr/lib:/home/guyinatuxedo/lib:/usr/local/lib:.:.. gcc -I. -I.. -ansi -Wall -pedantic -fsigned-char -O2 -o tcutilex tcutilex.o  -L. -L.. -ltokyocabinet -lz -lbz2 -lpthread -lm -lc
gcc -c -I. -I.. -ansi -Wall -pedantic -fsigned-char -O2 tchdbex.c
LD_RUN_PATH=/lib:/usr/lib:/home/guyinatuxedo/lib:/usr/local/lib:.:.. gcc -I. -I.. -ansi -Wall -pedantic -fsigned-char -O2 -o tchdbex tchdbex.o  -L. -L.. -ltokyocabinet -lz -lbz2 -lpthread -lm -lc
gcc -c -I. -I.. -ansi -Wall -pedantic -fsigned-char -O2 tcbdbex.c
LD_RUN_PATH=/lib:/usr/lib:/home/guyinatuxedo/lib:/usr/local/lib:.:.. gcc -I. -I.. -ansi -Wall -pedantic -fsigned-char -O2 -o tcbdbex tcbdbex.o  -L. -L.. -ltokyocabinet -lz -lbz2 -lpthread -lm -lc
gcc -c -I. -I.. -ansi -Wall -pedantic -fsigned-char -O2 tcfdbex.c
LD_RUN_PATH=/lib:/usr/lib:/home/guyinatuxedo/lib:/usr/local/lib:.:.. gcc -I. -I.. -ansi -Wall -pedantic -fsigned-char -O2 -o tcfdbex tcfdbex.o  -L. -L.. -ltokyocabinet -lz -lbz2 -lpthread -lm -lc
gcc -c -I. -I.. -ansi -Wall -pedantic -fsigned-char -O2 tctdbex.c
LD_RUN_PATH=/lib:/usr/lib:/home/guyinatuxedo/lib:/usr/local/lib:.:.. gcc -I. -I.. -ansi -Wall -pedantic -fsigned-char -O2 -o tctdbex tctdbex.o  -L. -L.. -ltokyocabinet -lz -lbz2 -lpthread -lm -lc
gcc -c -I. -I.. -ansi -Wall -pedantic -fsigned-char -O2 tcadbex.c
LD_RUN_PATH=/lib:/usr/lib:/home/guyinatuxedo/lib:/usr/local/lib:.:.. gcc -I. -I.. -ansi -Wall -pedantic -fsigned-char -O2 -o tcadbex tcadbex.o  -L. -L.. -ltokyocabinet -lz -lbz2 -lpthread -lm -lc
LD_RUN_PATH=/lib:/usr/lib:/home/guyinatuxedo/lib:/usr/local/lib:.:.. gcc -I. -I.. -std=c99 -Wall -pedantic -fsigned-char -O2 -o tctchat.cgi tctchat.c  -L. -L.. -ltokyocabinet -lz -lbz2 -lpthread -lm -lc
LD_RUN_PATH=/lib:/usr/lib:/home/guyinatuxedo/lib:/usr/local/lib:.:.. gcc -I. -I.. -std=c99 -Wall -pedantic -fsigned-char -O2 -o tctsearch.cgi tctsearch.c  -L. -L.. -ltokyocabinet -lz -lbz2 -lpthread -lm -lc
```