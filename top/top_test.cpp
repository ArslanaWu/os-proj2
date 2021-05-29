#include <fstream>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <regex>
#include <unistd.h>
#include <sys/resource.h>
#include <pwd.h>
#include <dirent.h>
#include <iomanip>
#include <csignal>
#include <sys/ioctl.h>
#include <queue>
#include <stack>
#include <termios.h>

const int PAGESIZE = getpagesize() / 1024; //// size in KB
const long CLOCK_TICKS = sysconf(_SC_CLK_TCK);
int total_process = 0, running_process = 0, sleeping_process = 0, stopped_process = 0;
static bool _kbhit_initialized = false;

struct process {
    std::string pid;
    std::unordered_map<std::string, int> statm;
    std::unordered_map<std::string, std::string> stat;
    int nice;

    process(const std::string &pid,
            const std::unordered_map<std::string, int> &statm,
            const std::unordered_map<std::string, std::string> &stat,
            int nice) {
        this->pid = pid;
        this->statm = statm;
        this->stat = stat;
        this->nice = nice;
    }
};

struct compare_process {
    int field;

    compare_process(int field = 0) : field(field) {}

    bool operator()(const process &lhs, const process &rhs) const {
        std::unordered_map<std::string, int> lhs_statm = lhs.statm;
        std::unordered_map<std::string, int> rhs_statm = rhs.statm;
        switch (field) {
            case 0:
                return lhs_statm["vms"] > rhs_statm["vms"];
            case 1:
                return lhs_statm["rss"] > rhs_statm["rss"];
            case 2:
                return lhs_statm["shared"] > rhs_statm["shared"];
        }
        return true;
    }
};

/**
 * split input by regex
 *
 * @param input
 * @param regex
 * @return
 */
std::vector<std::string>
split(const std::string &input, const std::string &regex) {
    // passing -1 as the submatch index parameter performs splitting
    std::regex re(regex);
    std::sregex_token_iterator
            first{input.begin(), input.end(), re, -1},
            last;
    return {first, last};
}

/**
 * add "0" before str until length of str equals total_len
 *
 * @return filled string
 */
std::string zfill(std::string str, int total_len) {
    while (str.length() < total_len) {
        str = "0" + str;
    }
    return str;
}

/**
 * check if a string is a number
 *
 * @return true or false
 */
bool is_digit(const std::string &str) {
    for (char i : str) {
        if (!std::isdigit(i)) {
            return false;
        }
    }
    return true;
}

/**
 * reset settings after ctrl+c is pressed
 *
 * @param signum
 */
void SIGINT_handler(int signum) {
    printf("\033[?25h"); // re-enable cursor
    exit(signum);
}

/**
 * check if there is something in the input buffer
 *
 * @return true or false
 */
int _kbhit() {
    static const int STDIN = 0;

    if (!_kbhit_initialized) {
        // Use termios to turn off line buffering
        termios term;
        tcgetattr(STDIN, &term);
        term.c_lflag &= ~ICANON;
        tcsetattr(STDIN, TCSANOW, &term);
        setbuf(stdin, NULL);
        _kbhit_initialized = true;
    }

    int bytesWaiting;
    ioctl(STDIN, FIONREAD, &bytesWaiting);
    return bytesWaiting;
}

/**
 * read /proc/meminfo
 *
 * @return map stores mem name and size, size in KB
 */
std::unordered_map<std::string, int>
read_meminfo() {
    std::unordered_map<std::string, int> mem_map;

    std::ifstream fin;
    fin.open("/proc/meminfo");
    std::string line;
    while (!fin.eof()) {
        getline(fin, line);

        std::string mem_name = line.substr(0, line.find(':'));
        int mem_size = 0;
        for (char i : line) {
            if (std::isdigit(i)) {
                mem_size = mem_size * 10 + i - 48;
            }
        }
        mem_map[mem_name] = mem_size;
    }
    fin.close();

    return mem_map;
}

/**
 * read /proc/vmstat
 *
 * @return sin and sout
 */
std::pair<int, int>
read_vmstat() {
    int sin = -1, sout = -1;
    std::ifstream fin;
    fin.open("/proc/vmstat");
    std::string line;
    while (!fin.eof() || sin == -1 || sout == -1) {
        getline(fin, line);

        if (line.find("pswpin") != std::string::npos) {
            sin = stoi(line.substr(line.find(' ') + 1)) * 4 * 1024;
        } else if (line.find("pswpout") != std::string::npos) {
            sout = stoi(line.substr(line.find(' ') + 1)) * 4 * 1024;
        }
    }
    fin.close();

    return std::make_pair(sin, sout);
}

/**
 * read /proc/pid/stat
 *
 * @param pid
 * @return map stores info type and info
 */
std::unordered_map<std::string, std::string>
read_pid_stat(const std::string &path) {
    std::ifstream fin;
    fin.open(path);
    std::string line;
    std::string next_line;
    while (!fin.eof()) {
        getline(fin, next_line);
        line += next_line;
    }
    fin.close();

    std::unordered_map<std::string, std::string> result_map;

    unsigned long name_start = line.find('(') + 1;
    unsigned long name_end = line.find(')');
    result_map["name"] = line.substr(name_start, name_end - name_start);

    std::vector<std::string> info = split(line.substr(name_end + 2), "\\s+");
    result_map["status"] = info[0];
    result_map["ppid"] = info[1];
    result_map["ttynr"] = info[4];
    result_map["utime"] = info[11];
    result_map["stime"] = info[12];
    result_map["children_utime"] = info[13];
    result_map["children_stime"] = info[14];
    result_map["create_time"] = info[19];
    result_map["cpu_num"] = info[36];
    result_map["blkio_ticks"] = info[39];

    double ctime = (stod(info[11]) + stod(info[12])
                    + stod(info[13]) + stod(info[14])) / CLOCK_TICKS;
    int minutes = (long) ctime / 60;
    int seconds = (long) ctime % 60;
    int hundredths = (ctime - (long) ctime) * 100;
    std::string ctime_str = std::to_string(minutes) + ":"
                            + zfill(std::to_string(seconds), 2) + ":"
                            + zfill(std::to_string(hundredths), 2);
    result_map["total_cpu_time"] = ctime_str;

    return result_map;
}

/**
 * read /proc/pid/statm
 *
 * @param pid
 * @return map stores mem name and size, mem size in KB
 */
std::unordered_map<std::string, int>
read_pid_statm(const std::string &path) {
    std::ifstream fin;
    fin.open(path);
    std::string line;
    std::string next_line;
    while (!fin.eof()) {
        getline(fin, next_line);
        line += next_line;
    }
    fin.close();

    std::vector<std::string> info = split(line, "\\s+");
    std::unordered_map<std::string, int> result_map;
    result_map["vms"] = stoi(info[0]) * PAGESIZE;
    result_map["rss"] = stoi(info[1]) * PAGESIZE;
    result_map["shared"] = stoi(info[2]) * PAGESIZE;
    result_map["text"] = stoi(info[3]) * PAGESIZE;
    result_map["lib"] = stoi(info[4]) * PAGESIZE;
    result_map["data"] = stoi(info[5]) * PAGESIZE;
    result_map["dirty"] = stoi(info[6]) * PAGESIZE;

    return result_map;
}

/**
 * @return map stores mem name and size and mem_percent, size in KB
 */
std::unordered_map<std::string, int>
virtual_mem() {
    std::unordered_map<std::string, int> mem_map = read_meminfo();

    int mem_total, mem_free, mem_avail, mem_buffers, mem_cached,
            mem_shared, mem_active, mem_inactive, mem_slab, mem_used;
    double mem_percent;

    mem_total = mem_map["MemTotal"]; // top line4 total
    mem_free = mem_map["MemFree"]; // top line4 free
    mem_cached = mem_map["Cached"]; // part of top line4 buff/cache
    mem_buffers = mem_map["Buffers"]; // part of top line4 buff/cache
    mem_shared = mem_map["Shmem"];
    mem_active = mem_map["Active"];
    mem_inactive = mem_map["Inactive"];
    mem_slab = mem_map["Slab"];

    mem_used = mem_total - mem_free - mem_cached - mem_buffers; // top line4 used
    if (mem_used < 0) {
        mem_used = mem_total - mem_free;
    }

    mem_avail = mem_map["MemAvailable"]; // top line5 avail mem
    if (mem_avail < 0) {
        mem_avail = 0;
    } else if (mem_avail > mem_total) {
        mem_avail = mem_free;
    }

    mem_percent = (double) (mem_used - mem_avail) / mem_total;

    std::unordered_map<std::string, int> result_map;
    result_map["total"] = mem_total;
    result_map["free"] = mem_free;
    result_map["cached"] = mem_cached;
    result_map["buffers"] = mem_buffers;
    result_map["used"] = mem_used;
    result_map["avail"] = mem_avail;

    return result_map;
}

/**
 * @return map stores mem name and size and mem_percent, size in KB
 */
std::unordered_map<std::string, int>
swap_mem() {
    std::unordered_map<std::string, int> mem_map = read_meminfo();

    int mem_total, mem_free, mem_used;
    double mem_percent;

    mem_total = mem_map["SwapTotal"]; // top line5 total
    mem_free = mem_map["SwapFree"]; // top line5 free
    mem_used = mem_total - mem_free; // top line5 used
    mem_percent = (double) mem_used / mem_total;

    std::pair<int, int> pair = read_vmstat();

    std::unordered_map<std::string, int> result_map;
    result_map["total"] = mem_total;
    result_map["free"] = mem_free;
    result_map["used"] = mem_used;
    result_map["sin"] = pair.first;
    result_map["sout"] = pair.second;

    return result_map;
}

/**
 * read /proc/pid/status to get owner of process
 *
 * @param pid pid of process
 * @return owner
 */
std::string get_process_owner(const std::string &pid) {
    std::ifstream fin;
    fin.open("/proc/" + pid + "/status");
    std::string line;
    while (line.find("Uid") == std::string::npos) {
        getline(fin, line);
    }
    fin.close();

    std::vector<std::string> info = split(line, "\\s+");
    return getpwuid(stoi(info[2]))->pw_name;
}

double get_boot_time() {
    std::ifstream fin;
    fin.open("/proc/stat");
    std::string line;
    while (line.find("btime") == std::string::npos) {
        getline(fin, line);
    }
    fin.close();
    return stoi(split(line, "\\s+")[1]);
}

/**
 * update number of each status of processes
 *
 * @param status process status
 */
void update_process_cnt(const std::string &status) {
    total_process++;
    if (status == "S" || status == "D") {
        sleeping_process++;
    } else if (status == "R") {
        running_process++;
    } else if (status == "T") {
        stopped_process++;
    }
}

/**
 * print global mem info and title of process memory table
 *
 * @param v_mem map of virtual memory info
 * @param s_mem map of swap memory info
 * @param thread_mode show thread or not
 */
void print_top_title(std::unordered_map<std::string, int> &v_mem,
                     std::unordered_map<std::string, int> &s_mem,
                     int thread_mode) {
    std::string name = thread_mode == 0 ? "Processes: " : "Threads: ";
    std::cout << setiosflags(std::ios::left)
              << std::setw(10) << name << resetiosflags(std::ios::left)
              << std::setw(5) << total_process << " total,"
              << std::setw(5) << running_process << " running,"
              << std::setw(5) << sleeping_process << " sleeping,"
              << std::setw(5) << stopped_process << " stopped"
              << std::endl;

    std::cout << setiosflags(std::ios::left)
              << std::setw(10) << "MiB Mem: " << resetiosflags(std::ios::left)
              << std::setw(10) << v_mem["total"] << " total,"
              << std::setw(10) << v_mem["free"] << " free,"
              << std::setw(10) << v_mem["used"] << " used,"
              << std::setw(10) << v_mem["buffers"] + v_mem["cached"] << " buff/cache"
              << std::endl;

    std::cout << std::setw(10) << "MiB Swap: "
              << std::setw(10) << s_mem["total"] << " total,"
              << std::setw(10) << s_mem["free"] << " free,"
              << std::setw(10) << s_mem["used"] << " used,"
              << std::setw(10) << v_mem["avail"] << " avail Mem"
              << std::endl;

    std::cout << std::endl;

    name = thread_mode == 0 ? "PID" : "TID";
    std::cout << setiosflags(std::ios::right)
              << std::setw(7) << name << " " << resetiosflags(std::ios::right)
              << setiosflags(std::ios::left)
              << std::setw(10) << "USER" << resetiosflags(std::ios::left)
              << setiosflags(std::ios::right)
              << std::setw(4) << "PR" << " "
              << std::setw(4) << "NI" << " "
              << std::setw(8) << "VIRT" << " "
              << std::setw(7) << "RES" << " "
              << std::setw(7) << "SHR" << " "
              << std::setw(8) << "%MEM" << " "
              << std::setw(11) << "TIME+" << " " << resetiosflags(std::ios::right)
              << setiosflags(std::ios::left)
              << std::setw(10) << "COMMAND" << resetiosflags(std::ios::left)
              << std::endl;
}

/**
 * print memory info of each process or thread
 *
 * @param p process struct stores mem info
 * @param mem_total total memory size
 * @param thread_mode show thread or not
 */
void print_top_line(const process &p, int mem_total, int thread_mode) {
    std::string pid = p.pid;
    int nice = p.nice;
    std::unordered_map<std::string, int> statm = p.statm;
    std::unordered_map<std::string, std::string> stat = p.stat;

    std::cout << setiosflags(std::ios::right)
              << std::setw(7) << pid << " " << resetiosflags(std::ios::right)
              << setiosflags(std::ios::left)
              << std::setw(10) << get_process_owner(pid) << resetiosflags(std::ios::left)
              << setiosflags(std::ios::right)
              << std::setw(4) << nice + 20 << " "
              << std::setw(4) << nice << " "
              << std::setw(8) << statm["vms"] << " "
              << std::setw(7) << statm["rss"] << " "
              << std::setw(7) << statm["shared"] << " "
              << stat["status"] << " "
              << setiosflags(std::ios::fixed) << std::setprecision(1)
              << std::setw(6) << (double) statm["rss"] / mem_total * 100 << " "
              << std::setw(11) << stat["total_cpu_time"] << " " << resetiosflags(std::ios::right)
              << setiosflags(std::ios::left)
              << std::setw(10) << stat["name"] << resetiosflags(std::ios::left);
}

/**
 * get priority queue stores top-r processes
 *
 * @param r size of priority queue
 * @param field define comparator
 * @param pid_list if not empty, only return top-r of these processes (except not exist processes)
 * @return result priority queue stores top-r processes
 */
std::priority_queue<process, std::vector<process>, compare_process>
get_top_R_process(int r, int field, const std::vector<std::string> &pid_list) {
    compare_process cmp(field);
    std::priority_queue<process, std::vector<process>, compare_process> pq(cmp);

    std::string task_dir = "/proc";
    const char *d = task_dir.c_str();
    if (auto dir = opendir(d)) {
        while (auto f = readdir(dir)) {
            if (is_digit(f->d_name)) {
                std::string pid = f->d_name;

                if (!pid_list.empty() && std::find(pid_list.begin(), pid_list.end(), pid) == pid_list.end()) {
                    continue;
                }

                std::unordered_map<std::string, int> statm = read_pid_statm("/proc/" + pid + "/statm");
                std::unordered_map<std::string, std::string> stat = read_pid_stat("/proc/" + pid + "/stat");
                int nice = getpriority(PRIO_PROCESS, stoi(pid));

                update_process_cnt(stat["status"]);

                if (pq.size() >= r) {
                    std::unordered_map<std::string, int> top_statm = pq.top().statm;
                    if (statm["rss"] > top_statm["rss"]) {
                        pq.pop();
                    }
                }

                if (pq.size() < r) {
                    pq.push(process(f->d_name, statm, stat, nice));
                }
            }
        }
        closedir(dir);
    }

    return pq;
}

/**
 * get priority queue stores top-r threads
 *
 * @param r size of priority queue
 * @param field define comparator
 * @param p return top-r threads of this process
 * @return result priority queue stores top-r threads
 */
std::priority_queue<process, std::vector<process>, compare_process>
get_top_R_task(int r, int field, const process &p) {
    compare_process cmp(field);
    std::priority_queue<process, std::vector<process>, compare_process> pq(cmp);

    std::string task_dir = "/proc/" + p.pid + "/task";
    const char *d = task_dir.c_str();
    if (auto dir = opendir(d)) {
        while (auto f = readdir(dir)) {
            if (is_digit(f->d_name)) {
                std::string tid = f->d_name;

                std::unordered_map<std::string, int> statm = read_pid_statm(
                        "/proc/" + p.pid + "/task/" + tid + "/statm");
                std::unordered_map<std::string, std::string> stat = read_pid_stat(
                        "/proc/" + p.pid + "/task/" + tid + "/stat");
                int nice = getpriority(PRIO_PROCESS, stoi(tid));

                update_process_cnt(stat["status"]);

                if (pq.size() >= r) {
                    std::unordered_map<std::string, int> top_statm = pq.top().statm;
                    if (statm["rss"] > top_statm["rss"]) {
                        pq.pop();
                    }
                }
                if (pq.size() < r) {
                    pq.push(process(f->d_name, statm, stat, nice));
                }
            }
        }
        closedir(dir);
    }

    return pq;
}

/**
 * print usage of these program
 */
void print_usage() {
    std::cout << "Usage: press [options] while running\n\n"
              << "Options:\n"
              << "d or D, delay time         specifies the delay between screen updates\n"
              << "t ot T, thread mode        structs top to display individual threads\n"
              << "o or O, change sort field  specifies the name of the field on which process will be sorted\n"
              << "p or P, monitor pid        monitor only processes with specified process IDs\n"
              << "h or H, help               print usage\n"
              << "q or Q, quit               quit top\n"
              << std::endl;
}

void top_main() {
    std::signal(SIGINT, SIGINT_handler); // reset command line settings
    system("stty -echo"); // hide command input

    // get window size
    struct winsize size;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &size);
    int r = size.ws_row - 1 - 4;

    // settings of top
    int thread_mode = 0; // 0: not show threads of processes
    int field = 0; // sort type
    int sleep_useconds = 500000; //sleep time
    std::vector<std::string> pid_list;

    std::stack<process> stack;

    while (true) {
        while (!_kbhit()) {
            printf("\033c"); // clear old
            printf("\033[?25l"); // hide cursor

            //init
            total_process = 0;
            running_process = 0;
            stopped_process = 0;
            sleeping_process = 0;

            // read global information
            std::unordered_map<std::string, int> v_mem = virtual_mem();
            std::unordered_map<std::string, int> s_mem = swap_mem();
            // read information for process
            std::priority_queue<process,
                    std::vector<process>, compare_process> pq = get_top_R_process(r, field, pid_list);
            if (thread_mode == 1 && !pq.empty()) {
                pq = get_top_R_task(r, field, pq.top());
            }

            // print global information
            print_top_title(v_mem, s_mem, thread_mode);
            // print process information
            while (!pq.empty()) {
                stack.push(pq.top());
                pq.pop();
            }
            while (!stack.empty()) {
                print_top_line(stack.top(), v_mem["total"], thread_mode);
                stack.pop();
                if (!stack.empty()) {
                    std::cout << std::endl;
                }
            }
            fflush(stdout);

            usleep(sleep_useconds);
        }

        // input detected
        int input = getchar();
        std::string invalid_input;
        system("/bin/stty cooked");
        system("stty echo"); // show command input

        if (input == 'd' || input == 'D') {
            // change sleep time
            printf("\033c"); // clear old
            double new_sleep_time = sleep_useconds / 1e6;
            std::cout << "Change sleep time from " << new_sleep_time << " to: ";
            std::cin >> new_sleep_time;
            if (std::cin.fail()) {
                std::cin.clear();
                std::cin >> invalid_input;
                std::cout << "Invalid input" << std::endl;
            } else if (new_sleep_time > 10 || new_sleep_time < 0) {
                std::cout << "Invalid input" << std::endl;
            } else {
                sleep_useconds = new_sleep_time * 1e6;
            }
        } else if (input == 't' || input == 'T') {
            // change thread mode
            if (thread_mode == 1) {
                thread_mode = 0;
                pid_list.clear();
            } else {
                printf("\033c"); // clear old
                std::cout << "Input pid: ";
                int pid;
                std::cin >> pid;
                if (std::cin.fail()) {
                    std::cin.clear();
                    std::cin >> invalid_input;
                    std::cout << "Invalid input" << std::endl;
                } else if (pid < 1) {
                    std::cout << "Invalid input" << std::endl;
                } else {
                    pid_list.clear();
                    pid_list.push_back(std::to_string(pid));
                    thread_mode = 1;
                }
            }
        } else if (input == 'o' || input == 'O') {
            // change sort mode
            printf("\033c"); // clear old
            std::string field_name;
            std::cout << "Input field want to sort, VIRT, RES or SHR: ";
            std::cin >> field_name;
            if (field_name == "VIRT") {
                field = 0;
            } else if (field_name == "RES") {
                field = 1;
            } else if (field_name == "SHR") {
                field = 2;
            } else {
                std::cout << "Invalid input" << std::endl;
            }
        } else if (input == 'p' || input == 'P') {
            if (thread_mode == 0 && !pid_list.empty()) {
                // exit monitor mode
                pid_list.clear();
            } else {
                // get monitor pid
                printf("\033c"); // clear old
                std::vector<std::string> new_pid_list;
                int pid;
                std::cout << "Next pid, " << "enter -1 to stop (max amount " << r << "): ";
                std::cin >> pid;
                while (pid_list.size() < r) {
                    if (std::cin.fail()) {
                        std::cin.clear();
                        std::cin >> invalid_input;
                        std::cout << "Invalid input" << std::endl;
                    } else if (pid == -1) {
                        break;
                    } else if (pid < 1) {
                        std::cout << "Invalid input" << std::endl;
                    } else {
                        new_pid_list.push_back(std::to_string(pid));
                    }
                    std::cout << "Next pid, " << "enter -1 to stop (max amount " << r << "): ";
                    std::cin >> pid;
                }

                if (!new_pid_list.empty()) {
                    if (thread_mode == 1) {
                        thread_mode = 0;
                    }
                    pid_list.clear();
                    pid_list.insert(pid_list.end(), new_pid_list.begin(), new_pid_list.end());
                }
            }
        } else if (input == 'h' || input == 'H') {
            printf("\033c"); // clear old
            print_usage();
            std::cout << "Input q or Q to quit: ";
            std::string quit_input;
            std::cin >> quit_input;
            while (quit_input != "q" && quit_input != "Q") {
                std::cout << "Input q or Q to quit: ";
                std::cin >> quit_input;
            }
        } else if (input == 'q' || input == 'Q') {
            printf("\033[?25h"); // re-enable cursor
            return;
        }

        _kbhit_initialized = false;
        system("stty -echo"); // hide command input
    }
}

//    system ("/bin/stty raw");
//    while((c=getchar())!= '.') {
//        putchar(c);
//    }
//    system ("/bin/stty cooked");