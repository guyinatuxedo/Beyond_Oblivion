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

## Table Creation

## Table Deletion

## Table Truncation

## Record Insertion

## Record Selection

## Record Deletion

## Record Update