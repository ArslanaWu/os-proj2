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

###### backtrace info looks like this:

```
./libmem.so(+0x150b) [0x7f39eb42f50b]  ./libmem.so(+0xec0) [0x7f39eb42eec0]  /usr/lib/x86_64-linux-gnu/libstdc++.so.6(+0x8f426) [0x7f39eb134426]  /lib64/ld-linux-x86-64.so.2(+0x108d3) [0x7f39eb6418d3]  /lib64/ld-linux-x86-64.so.2(+0x10ca) [0x7f39eb6320ca]  [(nil)]  [(nil)]  [(nil)] 
 ./libmem.so(+0x150b) [0x7f39eb42f50b]  ./libmem.so(+0xec0) [0x7f39eb42eec0]  /usr/lib/x86_64-linux-gnu/libstdc++.so.6(_Znwm+0x18) [0x7f39eb138298]  ./case1(main+0x12) [0x55bf1a6b19cc]  /lib/x86_64-linux-gnu/libc.so.6(__libc_start_main+0xe7) [0x7f39eacd5bf7]  ./case1(_start+0x2a) [0x55bf1a6b18da]  [(nil)]  [(nil)] 
 ./libmem.so(+0x150b) [0x7f39eb42f50b]  ./libmem.so(+0xec0) [0x7f39eb42eec0]  /usr/lib/x86_64-linux-gnu/libstdc++.so.6(_Znwm+0x18) [0x7f39eb138298]  ./case1(main+0x20) [0x55bf1a6b19da]  /lib/x86_64-linux-gnu/libc.so.6(__libc_start_main+0xe7) [0x7f39eacd5bf7]  ./case1(_start+0x2a) [0x55bf1a6b18da]  [(nil)]  [(nil)] 
 ./libmem.so(+0x150b) [0x7f39eb42f50b]  ./libmem.so(+0xec0) [0x7f39eb42eec0]  /usr/lib/x86_64-linux-gnu/libstdc++.so.6(_Znwm+0x18) [0x7f39eb138298]  ./case1(main+0x2e) [0x55bf1a6b19e8]  /lib/x86_64-linux-gnu/libc.so.6(__libc_start_main+0xe7) [0x7f39eacd5bf7]  ./case1(_start+0x2a) [0x55bf1a6b18da]  [(nil)]  [(nil)] 
 ./libmem.so(+0x150b) [0x7f39eb42f50b]  ./libmem.so(+0xec0) [0x7f39eb42eec0]  /usr/lib/x86_64-linux-gnu/libstdc++.so.6(_Znwm+0x18) [0x7f39eb138298]  ./case1(main+0x3c) [0x55bf1a6b19f6]  /lib/x86_64-linux-gnu/libc.so.6(__libc_start_main+0xe7) [0x7f39eacd5bf7]  ./case1(_start+0x2a) [0x55bf1a6b18da]  [(nil)]  [(nil)] 
 ./libmem.so(+0x150b) [0x7f39eb42f50b]  ./libmem.so(+0xec0) [0x7f39eb42eec0]  /usr/lib/x86_64-linux-gnu/libstdc++.so.6(_Znwm+0x18) [0x7f39eb138298]  ./case1(main+0x4a) [0x55bf1a6b1a04]  /lib/x86_64-linux-gnu/libc.so.6(__libc_start_main+0xe7) [0x7f39eacd5bf7]  ./case1(_start+0x2a) [0x55bf1a6b18da]  [(nil)]  [(nil)] 
```

###### For example: find code of :./case1(main+0x4a) [0x55bf1a6b1a04]

```
./case1(main+0x4a) [0x55bf1a6b1a04]
```

Open case1.map, search: .text

```
*(.text .stub .text.* .gnu.linkonce.t.*)
 .text          0x00000000000008b0       0x2b /usr/lib/gcc/x86_64-linux-gnu/7/../../../x86_64-linux-gnu/Scrt1.o
                0x00000000000008b0                _start
 .text          0x00000000000008db        0x0 /usr/lib/gcc/x86_64-linux-gnu/7/../../../x86_64-linux-gnu/crti.o
 *fill*         0x00000000000008db        0x5 
 .text          0x00000000000008e0       0xda /usr/lib/gcc/x86_64-linux-gnu/7/crtbeginS.o
 .text          0x00000000000009ba       0xc4 /tmp/cc2Uinxh.o
                0x00000000000009ba                main
 *fill*         0x0000000000000a7e        0x2 
 .text          0x0000000000000a80       0x72 /usr/lib/x86_64-linux-gnu/libc_nonshared.a(elf-init.oS)
                0x0000000000000a80                __libc_csu_init
                0x0000000000000af0                __libc_csu_fini
 .text          0x0000000000000af2        0x0 /usr/lib/gcc/x86_64-linux-gnu/7/crtendS.o
 .text          0x0000000000000af2        0x0 /usr/lib/gcc/x86_64-linux-gnu/7/../../../x86_64-linux-gnu/crtn.o
 *(.gnu.warning)
```

You can find the address of function 'main'

```
 .text          0x00000000000009ba       0xc4 /tmp/cc2Uinxh.o
                0x00000000000009ba                main
```

Add 0x09ba and 0x4a, you can find the final result: 0xa04

Now run this code, you can find the result.

```
addr2line -e ./case1 0xa04
```

