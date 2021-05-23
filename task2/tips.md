### Tips

##### 1. Compile：

```
gcc memtrace.c -g -fPIC -shared -rdynamic -o libmem.so -Wl,-Map,mem.map
```

```
g++ -g -rdynamic -o case1 case1.cpp -Wl,-Map,case1.map
```

##### 2. Test

```
LD_PRELOAD=./libmem.so ./case1
```

##### 3. Find 

```
addr2line -e ./case1 0x9ba(you should calculate this address according to case1.map and backtrace info.)
```

