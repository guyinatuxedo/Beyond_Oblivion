# Virtual Machine

These are some docs regarding the virtual machine implementation sqlite3 uses. The VM performs work by running opcodes (which are generated from parsing the sql query) in the `sqlite3VdbeExec` function, in a massive switch statement. I already covered in separate docs a lot of the functionality of various opcodes, so I won't have much here.

## OPcodes Listing

Here's a list of all of the opcodes:

```
OP_Savepoint       0
OP_AutoCommit      1
OP_Transaction     2
OP_SorterNext      3 /* jump                                       */
OP_Prev            4 /* jump                                       */
OP_Next            5 /* jump                                       */
OP_Checkpoint      6
OP_JournalMode     7
OP_Vacuum          8
OP_VFilter         9 /* jump, synopsis: iplan=r[P3] zplan='P4'     */
OP_VUpdate        10 /* synopsis: data=r[P3@P2]                    */
OP_Goto           11 /* jump                                       */
OP_Gosub          12 /* jump                                       */
OP_InitCoroutine  13 /* jump                                       */
OP_Yield          14 /* jump                                       */
OP_MustBeInt      15 /* jump                                       */
OP_Jump           16 /* jump                                       */
OP_Once           17 /* jump                                       */
OP_If             18 /* jump                                       */
OP_Not            19 /* same as TK_NOT, synopsis: r[P2]= !r[P1]    */
OP_IfNot          20 /* jump                                       */
OP_IfNullRow      21 /* jump, synopsis: if P1.nullRow then r[P3]=NULL, goto P2 */
OP_SeekLT         22 /* jump, synopsis: key=r[P3@P4]               */
OP_SeekLE         23 /* jump, synopsis: key=r[P3@P4]               */
OP_SeekGE         24 /* jump, synopsis: key=r[P3@P4]               */
OP_SeekGT         25 /* jump, synopsis: key=r[P3@P4]               */
OP_IfNotOpen      26 /* jump, synopsis: if( !csr[P1] ) goto P2     */
OP_IfNoHope       27 /* jump, synopsis: key=r[P3@P4]               */
OP_NoConflict     28 /* jump, synopsis: key=r[P3@P4]               */
OP_NotFound       29 /* jump, synopsis: key=r[P3@P4]               */
OP_Found          30 /* jump, synopsis: key=r[P3@P4]               */
OP_SeekRowid      31 /* jump, synopsis: intkey=r[P3]               */
OP_NotExists      32 /* jump, synopsis: intkey=r[P3]               */
OP_Last           33 /* jump                                       */
OP_IfSmaller      34 /* jump                                       */
OP_SorterSort     35 /* jump                                       */
OP_Sort           36 /* jump                                       */
OP_Rewind         37 /* jump                                       */
OP_IdxLE          38 /* jump, synopsis: key=r[P3@P4]               */
OP_IdxGT          39 /* jump, synopsis: key=r[P3@P4]               */
OP_IdxLT          40 /* jump, synopsis: key=r[P3@P4]               */
OP_IdxGE          41 /* jump, synopsis: key=r[P3@P4]               */
OP_RowSetRead     42 /* jump, synopsis: r[P3]=rowset(P1)           */
OP_Or             43 /* same as TK_OR, synopsis: r[P3]=(r[P1] || r[P2]) */
OP_And            44 /* same as TK_AND, synopsis: r[P3]=(r[P1] && r[P2]) */
OP_RowSetTest     45 /* jump, synopsis: if r[P3] in rowset(P1) goto P2 */
OP_Program        46 /* jump                                       */
OP_FkIfZero       47 /* jump, synopsis: if fkctr[P1]==0 goto P2    */
OP_IfPos          48 /* jump, synopsis: if r[P1]>0 then r[P1]-=P3, goto P2 */
OP_IfNotZero      49 /* jump, synopsis: if r[P1]!=0 then r[P1]--, goto P2 */
OP_IsNull         50 /* jump, same as TK_ISNULL, synopsis: if r[P1]==NULL goto P2 */
OP_NotNull        51 /* jump, same as TK_NOTNULL, synopsis: if r[P1]!=NULL goto P2 */
OP_Ne             52 /* jump, same as TK_NE, synopsis: IF r[P3]!=r[P1] */
OP_Eq             53 /* jump, same as TK_EQ, synopsis: IF r[P3]==r[P1] */
OP_Gt             54 /* jump, same as TK_GT, synopsis: IF r[P3]>r[P1] */
OP_Le             55 /* jump, same as TK_LE, synopsis: IF r[P3]<=r[P1] */
OP_Lt             56 /* jump, same as TK_LT, synopsis: IF r[P3]<r[P1] */
OP_Ge             57 /* jump, same as TK_GE, synopsis: IF r[P3]>=r[P1] */
OP_ElseNotEq      58 /* jump, same as TK_ESCAPE                    */
OP_DecrJumpZero   59 /* jump, synopsis: if (--r[P1])==0 goto P2    */
OP_IncrVacuum     60 /* jump                                       */
OP_VNext          61 /* jump                                       */
OP_Init           62 /* jump, synopsis: Start at P2                */
OP_PureFunc       63 /* synopsis: r[P3]=func(r[P2@NP])             */
OP_Function       64 /* synopsis: r[P3]=func(r[P2@NP])             */
OP_Return         65
OP_EndCoroutine   66
OP_HaltIfNull     67 /* synopsis: if r[P3]=null halt               */
OP_Halt           68
OP_Integer        69 /* synopsis: r[P2]=P1                         */
OP_Int64          70 /* synopsis: r[P2]=P4                         */
OP_String         71 /* synopsis: r[P2]='P4' (len=P1)              */
OP_Null           72 /* synopsis: r[P2..P3]=NULL                   */
OP_SoftNull       73 /* synopsis: r[P1]=NULL                       */
OP_Blob           74 /* synopsis: r[P2]=P4 (len=P1)                */
OP_Variable       75 /* synopsis: r[P2]=parameter(P1,P4)           */
OP_Move           76 /* synopsis: r[P2@P3]=r[P1@P3]                */
OP_Copy           77 /* synopsis: r[P2@P3+1]=r[P1@P3+1]            */
OP_SCopy          78 /* synopsis: r[P2]=r[P1]                      */
OP_IntCopy        79 /* synopsis: r[P2]=r[P1]                      */
OP_ChngCntRow     80 /* synopsis: output=r[P1]                     */
OP_ResultRow      81 /* synopsis: output=r[P1@P2]                  */
OP_CollSeq        82
OP_AddImm         83 /* synopsis: r[P1]=r[P1]+P2                   */
OP_RealAffinity   84
OP_Cast           85 /* synopsis: affinity(r[P1])                  */
OP_Permutation    86
OP_Compare        87 /* synopsis: r[P1@P3] <-> r[P2@P3]            */
OP_IsTrue         88 /* synopsis: r[P2] = coalesce(r[P1]==TRUE,P3) ^ P4 */
OP_Offset         89 /* synopsis: r[P3] = sqlite_offset(P1)        */
OP_Column         90 /* synopsis: r[P3]=PX                         */
OP_Affinity       91 /* synopsis: affinity(r[P1@P2])               */
OP_MakeRecord     92 /* synopsis: r[P3]=mkrec(r[P1@P2])            */
OP_Count          93 /* synopsis: r[P2]=count()                    */
OP_ReadCookie     94
OP_SetCookie      95
OP_ReopenIdx      96 /* synopsis: root=P2 iDb=P3                   */
OP_OpenRead       97 /* synopsis: root=P2 iDb=P3                   */
OP_OpenWrite      98 /* synopsis: root=P2 iDb=P3                   */
OP_OpenDup        99
OP_OpenAutoindex 100 /* synopsis: nColumn=P2                       */
OP_OpenEphemeral 101 /* synopsis: nColumn=P2                       */
OP_BitAnd        102 /* same as TK_BITAND, synopsis: r[P3]=r[P1]&r[P2] */
OP_BitOr         103 /* same as TK_BITOR, synopsis: r[P3]=r[P1]|r[P2] */
OP_ShiftLeft     104 /* same as TK_LSHIFT, synopsis: r[P3]=r[P2]<<r[P1] */
OP_ShiftRight    105 /* same as TK_RSHIFT, synopsis: r[P3]=r[P2]>>r[P1] */
OP_Add           106 /* same as TK_PLUS, synopsis: r[P3]=r[P1]+r[P2] */
OP_Subtract      107 /* same as TK_MINUS, synopsis: r[P3]=r[P2]-r[P1] */
OP_Multiply      108 /* same as TK_STAR, synopsis: r[P3]=r[P1]*r[P2] */
OP_Divide        109 /* same as TK_SLASH, synopsis: r[P3]=r[P2]/r[P1] */
OP_Remainder     110 /* same as TK_REM, synopsis: r[P3]=r[P2]%r[P1] */
OP_Concat        111 /* same as TK_CONCAT, synopsis: r[P3]=r[P2]+r[P1] */
OP_SorterOpen    112
OP_BitNot        113 /* same as TK_BITNOT, synopsis: r[P2]= ~r[P1] */
OP_SequenceTest  114 /* synopsis: if( cursor[P1].ctr++ ) pc = P2   */
OP_OpenPseudo    115 /* synopsis: P3 columns in r[P2]              */
OP_String8       116 /* same as TK_STRING, synopsis: r[P2]='P4'    */
OP_Close         117
OP_ColumnsUsed   118
OP_SeekScan      119 /* synopsis: Scan-ahead up to P1 rows         */
OP_SeekHit       120 /* synopsis: set P2<=seekHit<=P3              */
OP_Sequence      121 /* synopsis: r[P2]=cursor[P1].ctr++           */
OP_NewRowid      122 /* synopsis: r[P2]=rowid                      */
OP_Insert        123 /* synopsis: intkey=r[P3] data=r[P2]          */
OP_RowCell       124
OP_Delete        125
OP_ResetCount    126
OP_SorterCompare 127 /* synopsis: if key(P1)!=trim(r[P3],P4) goto P2 */
OP_SorterData    128 /* synopsis: r[P2]=data                       */
OP_RowData       129 /* synopsis: r[P2]=data                       */
OP_Rowid         130 /* synopsis: r[P2]=rowid                      */
OP_NullRow       131
OP_SeekEnd       132
OP_IdxInsert     133 /* synopsis: key=r[P2]                        */
OP_SorterInsert  134 /* synopsis: key=r[P2]                        */
OP_IdxDelete     135 /* synopsis: key=r[P2@P3]                     */
OP_DeferredSeek  136 /* synopsis: Move P3 to P1.rowid if needed    */
OP_IdxRowid      137 /* synopsis: r[P2]=rowid                      */
OP_FinishSeek    138
OP_Destroy       139
OP_Clear         140
OP_ResetSorter   141
OP_CreateBtree   142 /* synopsis: r[P2]=root iDb=P1 flags=P3       */
OP_SqlExec       143
OP_ParseSchema   144
OP_LoadAnalysis  145
OP_DropTable     146
OP_DropIndex     147
OP_DropTrigger   148
OP_IntegrityCk   149
OP_RowSetAdd     150 /* synopsis: rowset(P1)=r[P2]                 */
OP_Param         151
OP_Real          152 /* same as TK_FLOAT, synopsis: r[P2]=P4       */
OP_FkCounter     153 /* synopsis: fkctr[P1]+=P2                    */
OP_MemMax        154 /* synopsis: r[P1]=max(r[P1],r[P2])           */
OP_OffsetLimit   155 /* synopsis: if r[P1]>0 then r[P2]=r[P1]+max(0,r[P3]) else r[P2]=(-1) */
OP_AggInverse    156 /* synopsis: accum=r[P3] inverse(r[P2@P5])    */
OP_AggStep       157 /* synopsis: accum=r[P3] step(r[P2@P5])       */
OP_AggStep1      158 /* synopsis: accum=r[P3] step(r[P2@P5])       */
OP_AggValue      159 /* synopsis: r[P3]=value N=P2                 */
OP_AggFinal      160 /* synopsis: accum=r[P1] N=P2                 */
OP_Expire        161
OP_CursorLock    162
OP_CursorUnlock  163
OP_TableLock     164 /* synopsis: iDb=P1 root=P2 write=P3          */
OP_VBegin        165
OP_VCreate       166
OP_VDestroy      167
OP_VOpen         168
OP_VColumn       169 /* synopsis: r[P3]=vcolumn(P2)                */
OP_VRename       170
OP_Pagecount     171
OP_MaxPgcnt      172
OP_Trace         173
OP_CursorHint    174
OP_ReleaseReg    175 /* synopsis: release r[P1@P2] mask P3         */
OP_Noop          176
OP_Explain       177
OP_Abortable     178
```
