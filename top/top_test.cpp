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

/*
 * todo:
 * 1. try catch for invalid situation, e.g. file not found
 * 2. add memory info for threads (add an option?)
 * 3. provide more option to enable different sort methods, quantity of shown process
 * 4. remove unused values
 * 5. optimize structure of code, e.g. create all process struct at first, change mem value instead of create new maps
 * 6. beautify
 */

const int PAGESIZE = getpagesize() / 1024; //// size in KB
const long CLOCK_TICKS = sysconf(_SC_CLK_TCK);
int total_process = 0, running_process = 0, sleeping_process = 0, stopped_process = 0;

struct process {
    std::string pid;
    std::unordered_map<std::string, int> statm;
    std::unordered_map<std::string, std::string> stat;
    int nice;
//    std::unordered_map<std::string, std::unordered_map<std::string, int>> task_statm;

    process(const std::string &pid,
            const std::unordered_map<std::string, int> &statm,
            const std::unordered_map<std::string, std::string> &stat,
            int nice) {
        this->pid = pid;
        this->statm = statm;
        this->stat = stat;
        this->nice = nice;
    }

    bool operator<(const process &b) const {
        std::unordered_map<std::string, int> a_statm = statm;
        std::unordered_map<std::string, int> b_statm = b.statm;
        return a_statm["rss"] > b_statm["rss"];
    }
};

/**
 * split input by regex
 * @param input
 * @param regex
 * @return
 */
std::vector<std::string> split(const std::string &input, const std::string &regex) {
    // passing -1 as the submatch index parameter performs splitting
    std::regex re(regex);
    std::sregex_token_iterator
            first{input.begin(), input.end(), re, -1},
            last;
    return {first, last};
}

/**
 * add "0" before str until length of str equals total_len
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
 * reset after ctrl+c is pressed
 * @param signum
 */
void SIGINT_handler(int signum) {
    printf("\e[?25h"); // re-enable cursor
    exit(signum);
}

/**
 * read /proc/meminfo
 * @return map<mem name, size>, size in KB
 */
std::unordered_map<std::string, int> read_meminfo() {
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
 * @return
 */
std::pair<int, int> read_vmstat() {
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
 * @param pid
 * @return info_map
 */
std::unordered_map<std::string, std::string> read_pid_stat(const std::string &pid) {
    std::ifstream fin;
    fin.open("/proc/" + pid + "/stat");
    std::string line;
    while (line.empty()) {
        getline(fin, line);
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
 * mem size in KB
 * @param pid
 */
void read_pid_status(const std::string &pid) {
}

/**
 * read /proc/pid/statm
 * @param pid
 * @return mem_map <mem name, size>, mem size in KB
 */
std::unordered_map<std::string, int> read_pid_statm(const std::string &pid) {
    std::ifstream fin;
    fin.open("/proc/" + pid + "/statm");
    std::string line;
    while (line.empty()) {
        getline(fin, line);
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

void read_pid_smaps(const std::string &pid) {

}

std::unordered_map<std::string, std::unordered_map<std::string, int>> read_pid_task_statm(const std::string &pid) {
    std::unordered_map<std::string, std::unordered_map<std::string, int>> process_map;

    std::ifstream fin;
    std::string task_dir = "/proc/" + pid + "/task/";
    const char *p = task_dir.c_str();
    if (auto dir = opendir(p)) {
        while (auto f = readdir(dir)) {
            if (f->d_name[0] == '.') {
                continue; // Skip everything that starts with a dot
            } else {
                fin.open(task_dir + f->d_name + "/statm");
                std::string line;
                while (line.empty()) {
                    getline(fin, line);
                }
                fin.close();

                std::vector<std::string> info = split(line, "\\s+");
                std::unordered_map<std::string, int> task_map;
                task_map["vms"] = stoi(info[0]) * PAGESIZE;
                task_map["rss"] = stoi(info[1]) * PAGESIZE;
                task_map["shared"] = stoi(info[2]) * PAGESIZE;
                task_map["text"] = stoi(info[3]) * PAGESIZE;
                task_map["lib"] = stoi(info[4]) * PAGESIZE;
                task_map["data"] = stoi(info[5]) * PAGESIZE;
                task_map["dirty"] = stoi(info[6]) * PAGESIZE;

                process_map[f->d_name] = task_map;
            }
        }
        closedir(dir);
    }

    return process_map;
}

/**
 * @return mem_map and mem_percent, mem size in KB
 */
std::unordered_map<std::string, int> virtual_mem() {
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
 * @return mem_map and mem_percent, mem size in KB
 */
std::unordered_map<std::string, int> swap_mem() {
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

void print_top_title(std::unordered_map<std::string, int> &v_mem,
                     std::unordered_map<std::string, int> &s_mem) {
    std::cout << setiosflags(std::ios::left)
              << std::setw(10) << "Processes: " << resetiosflags(std::ios::left)
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
              << std::setw(10) << v_mem["avail"] << " avail"
              << std::endl;

    std::cout << std::endl;

    std::cout << setiosflags(std::ios::right)
              << std::setw(7) << "PID" << " " << resetiosflags(std::ios::right)
              << setiosflags(std::ios::left)
              << std::setw(10) << "USER" << resetiosflags(std::ios::left)
              << setiosflags(std::ios::right)
              << std::setw(4) << "PR" << " "
              << std::setw(4) << "NI" << " "
              << std::setw(8) << "VIRT" << " "
              << std::setw(7) << "RES" << " "
              << std::setw(6) << "%MEM" << " "
              << std::setw(11) << "TIME+" << " " << resetiosflags(std::ios::right)
              << setiosflags(std::ios::left)
              << std::setw(10) << "COMMAND" << resetiosflags(std::ios::left)
              << std::endl;
}

void print_top_line(const process &p, int mem_total) {
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
              << setiosflags(std::ios::fixed) << std::setprecision(1)
              << std::setw(6) << (double) statm["rss"] / mem_total * 100 << " "
              << std::setw(11) << stat["total_cpu_time"] << " " << resetiosflags(std::ios::right)
              << setiosflags(std::ios::left)
              << std::setw(10) << stat["name"] << resetiosflags(std::ios::left);
}

std::priority_queue<process> get_top_R_process(int r) {
    std::priority_queue<process> pq;

    std::string task_dir = "/proc";
    const char *d = task_dir.c_str();
    if (auto dir = opendir(d)) {
        while (auto f = readdir(dir)) {
            if (is_digit(f->d_name)) {
                std::string pid = f->d_name;

                std::unordered_map<std::string, int> statm = read_pid_statm(pid);
                std::unordered_map<std::string, std::string> stat = read_pid_stat(pid);;
                int nice = getpriority(PRIO_PROCESS, stoi(pid));

                total_process++;
                if (stat["status"] == "S" || stat["status"] == "D") {
                    sleeping_process++;
                } else if (stat["status"] == "R") {
                    running_process++;
                } else if (stat["status"] == "T") {
                    stopped_process++;
                }

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

int main() {
    std::signal(SIGINT, SIGINT_handler);
    int cnt = 10;

    printf("\e[?25l"); // hide cursor

    struct winsize size;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &size);
    int r = size.ws_row - 1 - 4;

    std::stack<process> stack;

    while (true) {
        printf("\033c"); // clear old
//        printf("\033[2J\033[%d;%dH", 0, 0);
        total_process = 0;
        running_process = 0;
        stopped_process = 0;
        sleeping_process = 0;

        std::unordered_map<std::string, int> v_mem = virtual_mem();
        std::unordered_map<std::string, int> s_mem = swap_mem();
        std::priority_queue<process> pq = get_top_R_process(r);//size.ws_row - 1

        print_top_title(v_mem, s_mem);
        while (!pq.empty()) {
            stack.push(pq.top());
            pq.pop();
        }
        while (!stack.empty()) {
            print_top_line(stack.top(), v_mem["total"]);
            stack.pop();
            if (!stack.empty()) {
                std::cout << std::endl;
            }
        }
//        std::cout << cnt;
        fflush(stdout);

        sleep(3);
    }
}

/*
 * top pid
 * top command =pid_stat["name"]
 * top pr =nice + 20
 * top nice =getpriority(self.pid)
 * Psutil.test() VSZ: top VIRT, =pid_statm["vms"]
 * Psutil.test() RSS: top RES, =pid_statm["rss"]
 * Psutil.test() %MEM: top %MEM, =pid_statm["rss"]/virtual_mem["total"]
 * create time =stod(stat["create_time"]) / CLOCK_TICKS + get_boot_time()
 */
