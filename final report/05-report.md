# Project2 Final Document

<div align=center>11811535 Enhuai Liu<br>11811535 Shangxuan Wu</div>

## Result Analysis

### Realize Real-time Statistics of Memory Usage

In the design document, we expect to

1. display information such as pid and three types of memory (physical, swap, and virtual memory) for  for each process.
2. provide the percentage of physical memory usage and process sorting functions.

So far, we have achieved **all the expected goals** in the design documents. The specific interface is shown in the FIGURE 1. In process mode, each row in the table represents a process. Column `PID`, `USER`, `PR`, `NI`, `TIME+` and `COMMAND` show information about pid, owner, priority, nice value, total CPU time and name. Column `VIRT`, `RES`, and `SHR` contain the virtual, physical and swap memory information of processes respectively. Column `%MEM` shows the percentage of the process’s physical memory usage.

![top](fig/top.png)

<center><b>FIGURE 1</b> Process mode of Memory Usage</center>

In addition to the above plan goals, we have also **implemented six enhancements**.

1. The first line in FIGURE 1 shows the global process operation, and the second and third lines show the global memory information. Running status of each process is also shown in the eighth column.

2. Input `p` of `P` to enter specific processes user want to monitor. Process will not be shown if the input process does not exist. An example is shown in FIGURE 2.

   ![ptest](fig/ptest.png)

   ![presult](fig/presult.png)

   <center><b>FIGURE 2</b> Process Selection</center>

3. Input `t` or `T` to monitor threads of one process. First line will be replaced to statistics of thread running status. An example is shown in FIGURE 3.

   ![ptest](fig/ttest.png)

   ![presult](fig/tresult.png)

   <center><b>FIGURE 3</b> Thread Mode</center>

4. Input `d` or `D` to change interval of refreshing.

5. Input `o` or `O` to change the type of memory used by sorting. FIGURE 1 sorting using `VIRT` while FIGURE 4 sorting using `RES`.

   ![ptest](fig/otest.png)

   ![presult](fig/oresult.png)

   <center><b>FIGURE 4</b> Sort by RES</center>

6. Input `h` or `H` to view a help page, which is shown in FIGURE 5.

   ![ptest](fig/htest.png)

   <center><b>FIGURE 5</b> Help Page</center>

### Realize Detection of Memory Allocation and Release

### Realize Detection of Memory Leaks

## Implementation

### Realize Real-time Statistics of Memory Usage

Information of global physical memory and swap memory can be get by analyzing `/proc/meminfo` and `/proc/vmstat`. They are stored in global variables with type `std::unordered_map`.

Traverse the `/proc` or `/proc/[pid]/task` folder helps us get all the currently existing processes or threads. To get information of processes, I read two files for each process: `/proc/[pid]/stat` and `/proc/[pid]/statm`. The first file contains information such as running status, and the second file contains memory usage. Similarly, same kind of information of threads can be obtained from `/proc/[pid]/task/[tid]/stat` and `/proc/[pid]/task/[tid]/statm`. A struct of processes and threads `program` is created to store information above using `std::unordered_map`.

```c++
struct program {
    std::string id;
    std::unordered_map<std::string, int> statm; // information in stat file
    std::unordered_map<std::string, std::string> stat; // information in statm file
    int nice; // nice value
};
```

While getting top-R processes or threads, we use `std::priority_queue` with a custom comparator  struct `compare_process`. It specify the type of memory used for sorting by acquiring parameter `field`.

```c++
struct compare_process {
    int field; // specify the type of memory used for sorting
    bool operator()(const program &lhs, const program &rhs) const {
        return lhs[field] > rhs[field];
    }
};
```

In main function, we obtain top-R threads or processes periodically and print them on the screen. If it detects that there is input in the input buffer, main function jumps out of the loop to process the input, and then re-enters the loop. The pseudo code is as follows.

```c++
int main(){
	while(true){
		while(has no input){
			get_global_mem_info();
			get_top_R_mem_info(int r);
			print();
		}
		// logic corresponding to the input
		sleep(t);
	}
}
```

### Realize Detection of Memory Allocation and Release

### Realize Detection of Memory Leaks

## Future Direction

### Realize Real-time Statistics of Memory Usage

1. Real-time calculation of CPU occupancy rate and the corresponding sorting comparator can be added to show the scheduling of the process more intuitively.

2. In order to respond more quickly to the letter instructions entered on the command line, multiple threads can be added to handle the display of thread memory and the interaction with the command line respectively. Through that, the commands on the command line will also be responded to the first time during the sleep period of the program.

3. Change the command line display to the web page display, which is not only more beautiful, but also easier to add more interactive component.

### Realize Detection of Memory Allocation and Release

### Realize Detection of Memory Leaks

## Summary

From this project, I learned three aspects of technology.

1. We have a better understanding of the environment configuration, file system details, processes, and thread organization of the Linux system. In addition, I am more familiar with the implementation of command-line related functions.
2. We have learned about situations of memory leaks in the C++ development process, and have an in-depth understanding memory-related functions such as `malloc` and memory allocation mechanisms. We are aware of the importance of memory detection and memory leak detection for the development of C++ programs, and know how to develop compliant code.
3. In the process of using C++ to complete the project, we used more system calls and library functions, learned about the relevant code specifications, and gained a deeper understanding of the C++ language.

The experience of teamwork made us feel the power of division of labor and cooperation and complementarity. Different teammates have different strengths. If they can accomplish what they are good at, the efficiency will be greatly improved. The setting of the team plan and timely update of the docking also play an important role in advancing the progress.

## Division of Labor

| Teammates    | Work                                                         |
| ------------ | ------------------------------------------------------------ |
| Enhuai Liu   | Learn dynamic link library technology and replace required memory-related functions. Finish implementation of dynamic memory detection and memory leak detection. |
| Shangxuan Wu | Extract and analyze required data from relevant documents, then design display front end. Finish implementation of real-time statistics of memory usage and interactive operation logic. Complete the construction of the tool main entrance. |

