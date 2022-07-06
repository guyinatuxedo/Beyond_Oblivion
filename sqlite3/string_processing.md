# String Processing

So this document describes how sqlite3 will parse a sql query, and generate the corresponding vdbe code which will actually execute the query.

So the function actually responsible for parsing the sql query is the `sqlite3RunParser` function.This function will use the `sqlite3GetToken` function to tokenize parts of the imputed sql. A few things, the max sql len is defined by `mxSqlLen`.

One thing to note. There is a `Parse` struct that is passed around to the various parsing functionality. It can be found in `sqlite3.c`.

## Tokenization

A lot of the tokenization will take place in `sqlite3GetToken` in the `tokenize.c` file. In the binary. This function operates off of a switch statement. We can see that the value being evaluated for the switch statement is the first character of the sql query:

```
int sqlite3GetToken(const unsigned char *z, int *tokenType){
  int i, c;
  switch( aiClass[*z] ){  /* Switch on the character-class of the first byte
                          ** of the token. See the comment on the CC_ defines
```

Now for the exact cases. We can see that the cases are defined as enums, such as `CC_DOLLAR` or `CC_KYWD0`. Looking here, we see the case for keywords such as `SELECT`. It calculates the length of the keyword, and then uses the `keywordCode` function in order to identify which keyword it is.

```
    case CC_KYWD0: {
      for(i=1; aiClass[z[i]]<=CC_KYWD; i++){}
      if( IdChar(z[i]) ){
        /* This token started out using characters that can appear in keywords,
        ** but z[i] is a character not allowed within keywords, so this must
        ** be an identifier instead */
        i++;
        break;
      }
      *tokenType = TK_ID;
      return keywordCode((char*)z, i, tokenType);
    }
```

Now how the tokenization will work, is it will parse a sql statement in parts. Take for instance this sql statement:

```
SELECT * FROM x;

Will be parsed in the order:
SELECT
*
FROM
x
```

So to take a look at how this statement will be tokenized (token enums defined in `sqlite3.c`):
```
"SELECT * FROM x;"
"SELECT":     0x89 (TK_SELECT)
" ":        0xb5 (TK_SPACE)
"*":        0x6c (TK_STAR)
" ":        0xb5 (TK_SPACE)
"FROM":     0x8d (TK_FROM)
" ":        0xb5 (TK_SPACE)
"x":        0x3b (TK_ID)
";":        0x01 (TK_SEMI)
```

So looking further into the functionality of `sqlite3Parser`. Now there are sub functions which will help parse and generate the code for various types of statements. For example, select statements will rely on `sqlite3Select` function to generate the actual code. Now these functions are called within `yy_reduce`. However which function will be called, is decided from a previous function call to `fts5yy_find_shift_action`, which takes the current and previous tokens to determine the action. So it relies on the tokens generated from lexing to determine which function like `sqlite3Select` to generate the vdbe code. The opcodes in those functions is added with these functions:

```
SQLITE_PRIVATE int sqlite3VdbeAddOp0(Vdbe*,int);
SQLITE_PRIVATE int sqlite3VdbeAddOp1(Vdbe*,int,int);
SQLITE_PRIVATE int sqlite3VdbeAddOp2(Vdbe*,int,int,int);
SQLITE_PRIVATE int sqlite3VdbeGoto(Vdbe*,int);
SQLITE_PRIVATE int sqlite3VdbeLoadString(Vdbe*,int,const char*);
SQLITE_PRIVATE void sqlite3VdbeMultiLoad(Vdbe*,int,const char*,...);
SQLITE_PRIVATE int sqlite3VdbeAddOp3(Vdbe*,int,int,int,int);
SQLITE_PRIVATE int sqlite3VdbeAddOp4(Vdbe*,int,int,int,int,const char *zP4,int);
SQLITE_PRIVATE int sqlite3VdbeAddOp4Dup8(Vdbe*,int,int,int,int,const u8*,int);
SQLITE_PRIVATE int sqlite3VdbeAddOp4Int(Vdbe*,int,int,int,int,int);
SQLITE_PRIVATE int sqlite3VdbeAddFunctionCall(Parse*,int,int,int,int,const FuncDef*,int);
SQLITE_PRIVATE void sqlite3VdbeEndCoroutine(Vdbe*,int);
```
