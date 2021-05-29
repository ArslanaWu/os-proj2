# Project2 Design Document

<div align=center>11811535 Enhuai Liu<br>11811535 Shangxuan Wu</div>

## Project Background and Description

### Project Background

In years past, memory leaks were a common occurrence when developers wrote C and C++. This is because the developer did not carefully check whether the memory requested by the process has been released. **Memory leak** is a type of resource leak that occurs when a computer program incorrectly manages memory allocations in a way that memory which is no longer needed is not released. There are no doubt that memory is limited resource. As a result, these programs will continue to apply for available memory, which will eventually cause the memory to run out and the program will stop. It is of a great importance to monitor memory usage and find memory leaks in time.

In addition, it is also very important and useful to detect **memory allocation and release**. It helps us understand the allocation and use of memory, memory occupancy of different parts in the program, which helped us optimize the code. There are already many widely-used tools, such as `MLeak` and `Mtrace`.

### Project Description

Considering that memory is pivotal during development of C/C++, in this project we want to implement tools that support real-time display of process memory, detect memory allocation and release, and report possible memory leaks. The specific implementation steps and expected goals will be introduced in detail below.


## Expected Goals

### Realize Real-time Statistics of Memory Usage

In this part, we will dynamically display the three memory states of each process. First is physical memory, a  limited resource where code and data must reside when executed or referenced. Next is the optional swap file, where modified (dirty) memory can be saved and later retrieved if too many demands are made on physical memory. Lastly we have virtual memory, a nearly unlimited resource serving the following goals. 

Except these, we will also display important information such as pid, name, user, etc., so that users can understand the current memory usage more clearly. Further analysis and relevant options will be conducted too, such as calculating the physical memory occupancy rate and showing it. In the end, we will sort the process in order to make the displayed information more useful.

### Realize Detection of Memory Allocation and Release

In this part, we will implement detection of memory allocation and release. When memory allocation and deallocation occur, relevant information will be recorded. When the memory is acquired or released, the size and location of the acquired memory and the number of related lines of code will be written to the output file or printed into console.

There are several existing tools such as `mtrace`, `memleak` and `memleax` which aims to solve similar problem. We plan to firstly implement a static version, similar to the function implemented by tool `mtrace` and `memleak`. Static means user should introduce our header file into the his file, use functions defined by us before and after codes he need to check. Result can be get after the process finished. Secondly, we will spare no effort to implement a dynamic version and make it as strong as `memleax`. Dynamic means that our tools do not require users to change their source code. Start our tool when the process is running, and the related memory allocation and release information listed above can be detected and printed out.

### Realize Detection of Memory Leaks

In this part, we will implement detection of memory leaks. On the basis of Goal 2, when we assert memory leaks occurs, the related information will be recorded and shown to users. The information to be displayed includes the leaked memory address and size, the number of lines of code that caused the memory leak, the call stack, and other information. Same as Goal 2, we plan to firstly implement a static version and try to implement a dynamic version if possible. In static version, In the static version, users need to change their source code to enable our service and the dynamic version does not.

## Implementation

### Realize Real-time Statistics of Memory Usage

Our goal is to get the detailed memory information of the process and calculate the occupancy rate. 

The main channels for obtaining memory information are files in the `/proc` folder. For example, `/proc/PID/status` contains name, state, pid, physical memory, swap file and virtual memory information, corresponding to `Name`, `State`, `PID`, `VmRSS`, `VmSwap` and `VmSize` a in the file. To further calculate memory information, `/proc/PID/map` contains the virtual address corresponding to the code segment, stack area, heap area, dynamic library, and kernel area of the process, and `/proc/PID/smaps` contains more detailed memory usage data for each partition. Through these documents, more accurate memory information will be obtained and counted.

To calculate the occupancy rate of physical memory, we should get the total physical memory of the system  `MemTotal` in `/proc/meminfo` at first. Because we can get the physical memory actually occupied by the process, i.e. `VmRSS`, the rate can be easily calculated by formula (1).
$$
rate=\frac{VmRSS}{MemTotal}\times 100
\tag{1}
$$

### Realize Detection of Memory Allocation and Release

Since there are several functions in C library that will memory allocation/release, such as `molloc()`, `free()`, `new()`, `delete()`... We plan to use some methods (macro invocations/hooks) to replace these functions with our own functions. For example, replace `molloc()` with `Mymolloc`. Every time a memory allocation/release occurs, these new functions added into new function will record it and store these information to a list. For example, when the program applies for memory, the new function will not only allocate the memory it needs, but also pack the location and size of the memory with the code location of the requested memory (that is, create a structure), and then store to a file and add it to the allocated memory list we maintain. 

In the static version, the detailed memory information in list will be output in the file after the end of the program, while in the dynamic version be displayed in the console in real time. In this way, we can see the memory allocation dynamically and also won't lose information.

### Realize Detection of Memory Leaks

After implementing detection of memory allocation and release, we have got a list of memory allocation information. We can get address, size,  last use time, pointers refer to this part of each allocated memory pieces. We can solve this problem according to these information.

There are some cases memory leaks may occurs:

1. No pointer refer to this piece of memory.
2. This piece of memory had not been used for a long time.

Solutions for these 2 cases:

1. We can solve it in same way like Java gc. Check through all memory pieces, if there is no pointer refer to this piece, we assert that memory leaks occurs. 
2. Since different program have different frequency of visiting memory, this case is more complex. We should manually set a time threshold for different program. if `current time` - `last use time` > `time threshold`, we assert that memory leaks occurs. This threshold will be carefully set after conducting research and experiments.


## Division of Labor

There are four weeks from 4/30 to 5/30. For each week, the specific goals and division of labor are as follows.

| Week  | Goal                                                         | Shangxuan                                                    | Enhuai                                                       |
| ----- | ------------------------------------------------------------ | ------------------------------------------------------------ | ------------------------------------------------------------ |
| week1 | Finish implementation of real-time statistics of memory usage | Extract and analyze the required data from relevant documents | Apply data, write styles and update logic                    |
| week2 | Try to rewrite related functions to verify the feasibility of the plan for task2 and task3 | Rewrite related functions of C language                      | Rewrite related functions of C++ language                    |
| week3 | Finish implementation of detection of memory allocation and release | Implement memory block linked list logic                     | Synthesize the functions completed before to realize the function of recording memory usage |
| week4 | Finish implementation of detection of memory leaks           | Complete memory leak detection logic based on memory linked list | Try to achieve dynamic memory leak detection                 |