# icecoal

So icecoal is a db implementation found at `https://github.com/ppml38/icecoal`. It primarily operates off of a CVS architecture. Databases are directories. Tables are files within those directories. These files are CSVs (Comma Seperated Files), with rows being records.

## Base Usage

So this database operates as a Python API. To use it, you basically call `icecoal.query()` with SQL Statements. Here is an example script, that will create a database, and insert records into it:

```
import icecoal

icecoal.query("create database db")
icecoal.query("create table db/tbl(col)")
icecoal.query("insert into db/tbl('entry0')")
```

Which when we run it, gives us this database file:

```
$	python3 test.py 
$	cat db/tbl 
col
entry0guyinatuxedo@ubuntu
```

## Parsing & DB Operation Handling

Now `icecoal.query` is the base function responsible for handling queries, which we find it in `icecoal/icecoal.py`:

```
def query(q,limit=0):
    """Parse and execute the query
    :param: Query string with mandatory input and header files.
    :return: Matching list of rows
    :return: -1 if error
    :return: Empty list if no rows match
    """
    try:
        result=execute_query(q)
        if limit>0:
            result[2]=result[2][:limit]
        elif limit<0:
            result[2]=result[2][limit:]
        else:
            pass
        return result
    except sqlerror as e:
        return [e.sqlcode,e.message,[]]
``` 

Looking at it, we can see it primarily wraps `execute_query`:

```
def execute_query(q):
    q+="#"
    state='0'
    temp=""
    templist=[]
    
    exp=''
    etree=None
    required_fields=[]
    csvfile,headfile='',''
    keyword=''
    keys=[]
    values=[]
    
    #For each character in a query
    end= False
    found=False
    i=0
    while i < len(q):
        #for every rule in rule table
        passthis=False
        if not end:
            for j in range(0,len(rule)):
                #if state is equal to current state and it allows current char
                if (rule[j][0]==state) and ((rule[j][1]=='any') or(q[i] in rule[j][1])):
                    #Execute required and stop rule search
                    found=True
                    state=rule[j][2]
                    if rule[j][3]=="pass":
                        passthis=True
                    elif rule[j][3]=='listpush':
                        templist.append(temp)
                        temp=''
                    elif rule[j][3]!='': #push #valuepush
                        temp+=q[i]
                        
                    if rule[j][4]=="req": #clean
                        required_fields.append(temp)
                        temp=''
                    elif rule[j][4]=="csv":
                        csvfile=temp
                        temp=''
                    elif rule[j][4]=="csv_select":
                        if csvfile=='' and temp!='':
                            csvfile=temp
                        keyword='select'
                        temp=''
                    elif rule[j][4]=="head":
                        if headfile=='' and temp!='':
                            headfile=temp
                        temp=''
                    elif rule[j][4]=="head_select":
                        headfile=temp
                        temp=''
                        keyword='select'
                    elif rule[j][4]=="exp_select":
                        exp=temp
                        temp=''
                        #Apply BODMAS Rule with proper paranthesis
                        exp=par(exp)
                        #Create tree
                        etree=create_exp_tree(exp)
                        keyword='select'
                    elif rule[j][4]=="delete":
                        exp=temp
                        temp=''
                        #Apply BODMAS Rule with proper paranthesis
                        exp=par(exp)
                        #Create tree
                        etree=create_exp_tree(exp)
                        keyword='delete'
                    elif rule[j][4]=="mkdb":
                        dbname=temp
                        keyword='mkdb'
                        temp=''
                    elif rule[j][4]=="drdb":
                        dbname=temp
                        keyword='drdb'
                        temp=''
                    elif rule[j][4]=="tablename":
                        tablename=temp
                        temp=''
                    elif rule[j][4]=="header":
                        header=temp
                        temp=''
                    elif rule[j][4]=="row":
                        templist.append(temp)
                        row=templist
                        temp=''
                        templist=[]
                    elif rule[j][4]=="mktable":
                        keyword="mktable"
                    elif rule[j][4]=="drtb":
                        tablename=temp
                        keyword='drtb'
                        temp=''
                    elif rule[j][4]=="trtb":
                        tablename=temp
                        keyword='trtb'
                        temp=''
                    elif rule[j][4]=="insert":
                        keyword='insert'
                        temp=''
                    elif rule[j][4]=="key":
                        keys.append(temp)
                        temp=''
                    elif rule[j][4]=="value":
                        values.append(temp)
                        temp=''
                    elif rule[j][4]=="exp_update":
                        exp=temp
                        temp=''
                        #Apply BODMAS Rule with proper paranthesis
                        exp=par(exp)
                        #Create tree
                        etree=create_exp_tree(exp)
                        keyword='update'
                    elif rule[j][4]=="updateall":
                        if temp!='':
                            values.append(temp)
                        keyword='update'
                        temp=''
                    if state=='end':
                        end=True
                    break
            if not found:
                raise sqlerror(-2,"Unexpected character "+q[i]+" at position "+str(i+1))
            else:
                found=False
        if passthis==True:
            pass
        else:
            i+=1
    # Execute the corresponding function inferred from the query
    if keyword=='select':
        rt=__select(required_fields,csvfile,headfile,etree)
        if rt==-1:
            return [-20,'Table does not exist',[]]
        elif rt==-2:
            return [-21,'Not a table name',[]]
        elif rt==-3:
            return [-22,'Header file does not exist',[]]
        elif rt==-4:
            return [-23,'Provided header is not a file',[]]
        else:
            return rt
    elif keyword=='mkdb':
        if __mkdb(dbname)==0:
            return [0,'Success',[]]
        else:
            return [-15,'Database already exists',[]]
    elif keyword=='mktable':
        rt=__mktable(tablename,header)
        if rt==0:
            return [0,'Success',[]]
        elif rt==-1:
            return [-16,'Table already exists',[]]
        elif rt==-2:
            return [-17,'Database does not exist',[]]
        elif rt==-3:
            return [-18,'Database name is blank',[]]
    elif keyword=='drdb':
        rt=__dropdb(dbname)
        if rt==0:
            return [0,'Success',[]]
        elif rt==-1:
            return [-17,'Database does not exist',[]]
        elif rt==-2:
            return [-19,'Not a database name',[]]
    elif keyword=='drtb':
        rt=__droptable(tablename)
        if rt==0:
            return [0,'Success',[]]
        elif rt==-1:
            return [-20,'Table does not exist',[]]
        elif rt==-2:
            return [-21,'Not a table name',[]]
    elif keyword=='trtb':
        rt=__truntable(tablename)
        if rt==0:
            return [0,'Success',[]]
        elif rt==-1:
            return [-20,'Table does not exist',[]]
        elif rt==-2:
            return [-21,'Not a table name',[]]
    elif keyword=='insert':
        rt=__insertrow(tablename,row)
        if rt==0:
            return [0,'Success',[]]
        elif rt==-1:
            return [-20,'Table does not exist',[]]
        elif rt==-2:
            return [-21,'Not a table name',[]]
        elif rt==-3:
            return [-24,'Values count does not match table fields count',[]]
    elif keyword=='update':
        rt=__update(keys,values,csvfile,headfile,etree)
        if rt==-1:
            return [-20,'Table does not exist',[]]
        elif rt==-2:
            return [-21,'Not a table name',[]]
        elif rt==-3:
            return [-22,'Header file does not exist',[]]
        elif rt==-4:
            return [-23,'Provided header is not a file',[]]
        else:
            return rt
    elif keyword=='delete':
        rt=__delete(csvfile,headfile,etree)
        if rt==-1:
            return [-20,'Table does not exist',[]]
        elif rt==-2:
            return [-21,'Not a table name',[]]
        elif rt==-3:
            return [-22,'Header file does not exist',[]]
        elif rt==-4:
            return [-23,'Provided header is not a file',[]]
        else:
            return rt
    else: #Keyword possibly blank
        return [-1,'Query incomplete',[]]
```

So looking at the first part (the while loop) we see that it parses the input sql statement. Then we see that after it gets the part where depending on what the action is, it will launch a corresponding function to handle that database operation. Here is a list of them:

```
__select
__mkdb
__mktable
__dropdb
__droptable
__truntable
__insertrow
__update
__delete
```

## Database Creation

So for database creation, this is handled by the `__mkdb` function from the `icecoal/utilfuns.py` file:

```
def __mkdb(dbname):
    '''Private function that creates a new database with given [path]name
    If dbname given with no path, it will be created in current directory
    @param: dbname - String Database name to be created
    @return: 0 if the database created
    @return: -1 if database already exists
    @throws: Exception if database could not be created due to environment or permission issues.
    '''
    if dbname !='':
        #If there is no such folder create one
        if not os.path.exists(dbname):
            os.makedirs(dbname)
            return 0
        #If directory already exists do nothing
        else:
            return -1
    #If the dbname is empty, do nothing
    else:
        pass
```

Looking at it, we can tell that it basically just makes the database directory, via creating a directory with the same name as the database.

## Database Deletion

So for database creation, this is handled by the `__dropdb` function from the `icecoal/utilfuns.py` file:

```
def __dropdb(dbname):
    '''Drops a database with all of its tables
    @param: dbname - String, name of the database to be deleted.
    @return: -1 if the database doesnt exist
    @return: -2 if the given dbname is not a directory name
    '''
    if os.path.exists(dbname):
        if os.path.isdir(dbname):
            shutil.rmtree(dbname)
            return 0
        else:
            return -2
    else:
        return -1
```

Looking at it, we can tell that it just deletes the directory, by deleing the directory with the sanme name as a database.

## Table Creation

So for table creation, this is handled by the `__mktable` function from the `icecoal/utilfuns.py` file:

```
def __mktable(tablename,header): #Optionally with path
    '''Creates a table with given path and header content
    If table name given with no path, it will be created in current directory
    @param: tablename - String name of table to be created
    @param: header - String header of the csv file
    @return: 0 if table creation successful
    @return: -1 if the table already exists
    @return: -2 if database does not exists
    @return: -3 Database name is blank
    @throws: Exception if the table could not be created due to environment issues or permission issues
    '''
    if ((tablename!='')&(header!='')):
        #If there is no such table create one
        if os.path.dirname(tablename)=='':
            return -3
        if (os.path.dirname(tablename)!='') and not os.path.exists(os.path.dirname(tablename)):
            return -2
        if not os.path.exists(tablename):
            newfile=open(tablename,'w')
            newfile.write((DELIMITER.join(header.split(","))))
            newfile.close()
            return 0
        #If table already exists do nothing return error code
        else:
            return -1
    #If empty table or header name passed, do nothing
    else:
        pass
```

Looking at it, we can see that it starts off by checking if the file named after the table exists. If it doesn't, it makes the table, and writes the header (the column names, seperated by a comma) to the file.


## Table Deletion

So for table deletion, this is handled by the `__droptable` function from the `icecoal/utilfuns.py` file:

```
def __droptable(tablename):
    '''Drops a table
    @param: table - String, name of the table to be deleted with its database name
    @return: -1 if the table does not exist
    @return: -2 if the given table name is not a table
    '''
    if os.path.exists(tablename):
        if os.path.isfile(tablename):
            os.remove(tablename)
            return 0
        else:
            return -2
    else:
        return -1
```

So we can see, for effectively deleting a table, it just deletes the table file.

## Table Truncation

So for table truncation, this is handled by the `__truntable` function from the `icecoal/utilfuns.py` file:

```
def __truntable(tablename):
    '''Truncates the table
    @param: table - String, name of the table to be truncated with the database name
    @return: -1 if the table does not exist
    @return: -2 if the given table name is not a table
    @return: Return codes of __mktable function
    '''
    f=None
    try:
        if os.path.exists(tablename):
            if os.path.isfile(tablename):
                f = open(tablename,'r')
                header=f.readline().strip('\n')
                f.close()
                
                f= open(tablename,'w')
                f.write(header)
                f.close()
                return 0
            else:
                return -2
        else:
            return -1
    except:
        raise
    finally:
        if f!=None:
            if not f.closed:
                f.close()
```

So for table truncation, we can see it just takes the table file, and overwrites it with a single line, being the table header. So it effectively just deletes all lines of the table file, past the header, thus deleting all records while retaining the same schema.

## Record Insertion

So for record insertion, this is handled by the `__insertrow` function from the `icecoal/utilfuns.py` file:

```
def __insertrow(tablename,row):
    '''Inserts given row in given table as a last row
    This inserts a new lineseparator specific to current os at every line end
    @param: tablename, string with dbname/tablename.csv format
    @param: row, string of comma separated values to be inserted in the table
    @return: 0 if the row inserted successfully
    @return: -1 if the table does not exist
    @return: -2 if given is not a table name
    @return: -3 if provided values count does not match with field count
    @throws: Exception if the operation could not be carried out due to other reasons
    '''
    givenfile=None
    try:
        if os.path.exists(tablename):
            if os.path.isfile(tablename):
                givenfile=open(tablename,'a+')
                
                #Check if provided field count matches with table's field count
                givenfile.seek(0)
                if (len(givenfile.readline().strip('\n').split(DELIMITER))) == (len(row)): #row will still be delimited by comma coming from query, we have to change to current DELIMITER when writing on file.
                
                    #It is important NOT TO use os.linesep here instead of \n. Because, by default, while 'writing' python will replace all \n chars
                    #to os.linesep. so if we use os.linesep instead of \n, that will translate into \r\n in windows and further \n will be replaced with
                    #os.linesep(i.e.,\r\n). So atlast you will get \r\r\n written in storage, which will then be read as empty line and a line separator in windows.
                    givenfile.write("\n"+(DELIMITER.join(row))) #As we are adding \n here python will replace this with os.linesep(\r\n). which is important to avoid
                                            #appending new lines with existing last line instead of next line
                else:
                    givenfile.close()
                    return -3
                
                givenfile.close()
                return 0
            else:
                return -2
        else:
            return -1
    except:
        raise
    finally:
        if givenfile!=None:
            if not givenfile.closed:
                givenfile.close()
```

So looking at this function, we see it first starts off with first checking if the table file exists. If it does, it will open it. Proceeding that, it will check if the record being inserted has the proper number of columns (via checking for `,` seperation), then if it does, it will append the line to the file to add the record.

## Record Selection

So for record selection, this is handled by the `__select` function from the `icecoal/utilfuns.py` file:

```
def __select(required_fields,csvfile,headfile,etree):
    '''Function that returns rows that match the condition given
    @param: requiired_fields-list of fields that need to be returned or *
    @param: csvfile-String of table name
    @param: headfile-String of header file name or empty
    @param: etree-Instance of Node. Root node of the expression tree created with given condition
    @return: 0 with result set or empty set if the query successfull
    @return: -1 if the table does not exist
    @return: -2 if given csvfile is not a name of a table
    @return: -3 if header file does not exist
    @return: -4 if header file is not name of a file
    @return: -5 field mentioned in select not found in table
    '''
    global DELIMITER
    #Initiate result set to empty
    result=[]
    count=0
    #Open table if exist
    if os.path.exists(csvfile):
        if os.path.isfile(csvfile):
            __csv_file=open(csvfile,'r')
        else:
            return -2
    else:
        return -1

    #open headfile if exist
    if headfile=='': # If no header file provided, first line of csvfile is the header line
        head=__csv_file.readline().strip('\n').split(DELIMITER)
    else:
        if os.path.exists(headfile):
            if os.path.isfile(headfile):
                hf=open(headfile,"r")
                head=hf.readline().strip('\n').split(DELIMITER)
                hf.close()
            else:
                return -4
        else:
            return -3
    line=__csv_file.readline()
    while(len(line)!=0): #Until end of file
        if len(line.strip('\n'))!=0: #If the line is empty line
            row=line.strip('\n').split(DELIMITER)
            exec_result=evalthis(etree,head,row)
            if(exec_result==True):
                if required_fields[0]=='*':
                    result.append(row)
                else:
                    temp_result=[]
                    for i in range(0,len(required_fields)):
                        found=False
                        for j in range(0,len(head)):
                            if head[j]==required_fields[i]:
                                found=True
                                if j<len(row):
                                    temp_result.append(row[j])
                                else:
                                    raise sqlerror(-12,"Value for variable "+head[j]+" is missing in atleast one row")
                                break
                        if not found:
                            raise sqlerror(-11,"No field "+required_fields[i]+" found in header")
                    result.append(temp_result)
            elif exec_result==False:
                pass
            else:
                raise sqlerror(-13,"Where clause condition returns non-boolean result")
        else:
            pass #Empty line, ignore it silently
        line=__csv_file.readline()
    __csv_file.close()
    count=len(result)
    if count==0:
        return [1,'0 rows selected',result]
    else:
        return [0,str(count)+' rows selected',result]
```

Looking at this, it starts off checking if the table file exists. Proceeding that, it will enter into a while loop, where it will iterate through each line of the database file. It will call the `evalthis` function to actually see if the row matches the search criteria. If it does, then it will attempt to parse out the rows. The returned rows are appended to the `result` array. If the rows requested is `*`, it will append the entire row to the `result` array. If not, it will iterate through each column, appending the desired columns via their index. 

## Record Deletion

So for record deletion, this is handled by the `__delete` function from the `icecoal/utilfuns.py` file:

```
def __delete(csvfile,headfile,etree):
    '''Deletes the row that matching condition
    @paramW: csvfile-String of data file name
    @param: headfile-String of header file name or empty
    @param: etree-Instance of Node. Root node of the expression tree created with given condition
    @return: 0 if the delete was successful
    @return: -1 if the table does not exist
    @return: -2 if given csvfile is not a name of a table
    @return: -3 if header file does not exist
    @return: -4 if header file is not name of a file
    @Throws: Exceptopm if any other error occured
    '''
    __csv_file=None
    __res_csv_file=None
    count=0
    try: #Execute the update process inside try, in case of any exception, Updates will be rolled back
        #Rename the table file to .bak
        #Open table if exist
        if os.path.exists(csvfile):
            if os.path.isfile(csvfile):
                os.rename(csvfile,csvfile+".bak")
                __csv_file=open(csvfile+".bak",'r')
            else:
                return -2
        else:
            return -1

        #open headfile if exist
        if headfile=='': # If no header file provided, first line of csvfile is the header line
            head=__csv_file.readline().strip('\n').split(DELIMITER)
        else:
            if os.path.exists(headfile):
                if os.path.isfile(headfile):
                    hf=open(headfile,"r")
                    head=hf.readline().strip('\n').split(DELIMITER)
                    hf.close()
                else:
                    return -4
            else:
                return -3
                
        #Open the resultant file with .bak2 extension. this will then be renamed to original csvfile once all updates performed
        __res_csv_file=open(csvfile+".bak2","w") #Note this may overwrite unfinished operation performed earlier
        #Write the header inside the file
        firstline=True
        if headfile=='':
            __res_csv_file.write(DELIMITER.join(head))
            firstline=False
        
        line=__csv_file.readline()
        while(len(line)!=0): #Until end of file
            if len(line.strip('\n'))!=0: #If the line is empty line
                row=line.strip('\n').split(DELIMITER)
                exec_result=evalthis(etree,head,row)
                if(exec_result==True): #Means this row should be updated before written in result file.
                    count+=1
                    pass #This line will not be written in the result file. Hence deleted
                elif exec_result==False:
                    if firstline:
                        __res_csv_file.write(line.strip('\n')) #Writing the read line back with no change. Note here while reading \n was at end of line but while writing its in front
                        firstline=False
                    else:
                        __res_csv_file.write("\n"+line.strip('\n')) #Writing the read line back with no change. Note here while reading \n was at end of line but while writing its in front
                        
                else:
                    raise sqlerror(-13,"Where clause condition returns non-boolean result")
            else:
                pass #Empty line, ignore it silently
            line=__csv_file.readline()
        __csv_file.close()
        __res_csv_file.close()
        os.remove(csvfile+".bak")
        os.rename(csvfile+".bak2",csvfile)
        if count==0:
            return [2,'0 rows deleted',[]]
        else:
            return [0,str(count)+' row(s) deleted',[]]
    except:
        raise #Every exception will be thrown back to inform users
    finally: #Check table health and Rollback if necessary
        if __csv_file!=None:
            if not __csv_file.closed:
                __csv_file.close()
        if __res_csv_file!=None:
            if not __res_csv_file.closed:
                __res_csv_file.close()
        if os.path.isfile(csvfile):
            pass #Table is healthy
        elif os.path.isfile(csvfile+".bak"): #Error occured before deleting backup file, hence restore backup
            os.rename(csvfile+".bak",csvfile)
        elif os.path.isfile(csvfile+".bak2"): #Error occured before renaming result file. hence proceed with rename
            os.rename(csvfile+".bak2",csvfile)
        #Exceptions in finally clause is unhandled to inform user
```

Looking at this, we see that it starts off via checking that the table file actually exists. Now how this deletion processs happens is this. It will make a seperate working file. Then it will iterate through every line of the database record, checking if they should be deleted with the `evalthis` function. If it shouldn't be deleted, it writes the record to the new file. If it should be deleted, it does nothing. Assuming this process goes smoothly, it just replaces the table file with the new file.

## Record Update

So for record updates, this is handled by the `__update` function from the `icecoal/utilfuns.py` file:

```
def __update(fields,values,csvfile,headfile,etree):
    '''Ipdates the table with given values
    @param: fields, list of fields that need to be changed
    @param: values, list of corresponding values of above given fields
    @param: csvfile-String of data file name
    @param: headfile-String of header file name or empty
    @param: etree-Instance of Node. Root node of the expression tree created with given condition
    @return: 0 if the update was successful
    @return: -1 if the table does not exist
    @return: -2 if given csvfile is not a name of a table
    @return: -3 if header file does not exist
    @return: -4 if header file is not name of a file
    @Throws: Exceptopm if any other error occured
    '''
    __csv_file=None
    __res_csv_file=None
    count=0
    try: #Execute the update process inside try, in case of any exception, Updates will be rolled back
        #Rename the table file to .bak
        #Open table if exist
        if os.path.exists(csvfile):
            if os.path.isfile(csvfile):
                os.rename(csvfile,csvfile+".bak")
                __csv_file=open(csvfile+".bak",'r')
            else:
                return -2
        else:
            return -1

        #open headfile if exist
        if headfile=='': # If no header file provided, first line of csvfile is the header line
            head=__csv_file.readline().strip('\n').split(DELIMITER)
        else:
            if os.path.exists(headfile):
                if os.path.isfile(headfile):
                    hf=open(headfile,"r")
                    head=hf.readline().strip('\n').split(DELIMITER)
                    hf.close()
                else:
                    return -4
            else:
                return -3
                
        #Open the resultant file with .bak2 extension. this will then be renamed to original csvfile once all updates performed
        __res_csv_file=open(csvfile+".bak2","w") #Note this may overwrite unfinished operation performed earlier
        #Write the header inside the file
        firstline=True
        if headfile=='':
            __res_csv_file.write(DELIMITER.join(head))
            firstline=False
        
        line=__csv_file.readline()
        while(len(line)!=0): #Until end of file
            if len(line.strip('\n'))!=0: #If the line is empty line
                row=line.strip('\n').split(DELIMITER)
                exec_result=evalthis(etree,head,row)
                if(exec_result==True): #Means this row should be updated before written in result file.
                    count+=1
                    for i in range(0,len(fields)):
                        field_found=False #To check if the field provided is part of table
                        for j in range(0,len(head)):
                            if head[j]==fields[i]:
                                field_found=True
                                if j<len(row): #check if the row contains value for given field
                                    row[j]=values[i]
                                else:
                                    raise sqlerror(-12,"Value for variable "+head[j]+" is missing in atleast one row")
                                break
                        if not field_found:
                            raise sqlerror(-11,"No field "+fields[i]+" found in header")
                    if firstline:
                        __res_csv_file.write(DELIMITER.join(row)) #without prepending new line
                        firstline=False
                    else:
                        __res_csv_file.write("\n"+(DELIMITER.join(row))) #with prepending new line
                elif exec_result==False:
                    if firstline:
                        __res_csv_file.write(line.strip('\n')) #Writing the read line back with no change. Note here while reading \n was at end of line but while writing its in front
                        firstline=False
                    else:
                        __res_csv_file.write("\n"+line.strip('\n')) #Writing the read line back with no change. Note here while reading \n was at end of line but while writing its in front
                        
                else:
                    raise sqlerror(-13,"Where clause condition returns non-boolean result")
            else:
                pass #Empty line, ignore it silently
            line=__csv_file.readline()
        __csv_file.close()
        __res_csv_file.close()
        os.remove(csvfile+".bak")
        os.rename(csvfile+".bak2",csvfile)
        if count==0:
            return [3,'0 rows updated',[]]
        else:
            return [0,str(count)+' row(s) updated',[]]
    except:
        raise #Every exception will be thrown back to inform users
    finally: #Check table health and Rollback if necessary
        if __csv_file!=None:
            if not __csv_file.closed:
                __csv_file.close()
        if __res_csv_file!=None:
            if not __res_csv_file.closed:
                __res_csv_file.close()
        if os.path.isfile(csvfile):
            pass #Table is healthy
        elif os.path.isfile(csvfile+".bak"): #Error occured before deleting backup file, hence restore backup
            os.rename(csvfile+".bak",csvfile)
        elif os.path.isfile(csvfile+".bak2"): #Error occured before renaming result file. hence proceed with rename
            os.rename(csvfile+".bak2",csvfile)
        #Exceptions in finally clause is unhandled to inform user
```

Looking at this, we see it's similar to record deletion. It will create a new file, where the updated rows get written to. It will use `evalthis` to determine if a record that it sees (as it's iterating through) needs to be updated. If it doesn't need to be updated, it will just write the row as is to the new file as is. If it does need to be updated, then it will change the rows it via iterating through each column of the record, and every field to update (nested for loop). Then it will write the updated rows to the new file. After it's done iterating, it will update the table file.

