#include "top/top_test.cpp"

int main() {
    std::string type;

    while (true) {
        printf("\033c"); // clear old
        std::cout << "Input m/M to monitor memory usage, "
                  << "d/D to detect memory dynamics and leaks, "
                  << "q/Q to quit: ";
        std::cin >> type;

        if (type == "m" || type == "M") {
            top_main();
        } else if (type == "d" || type == "D") {
            std::string file_name = "";

            std::cout << "Input file name: ";
            std::cin >> file_name;
            std::ifstream fin;
            fin.open(file_name);
            while (fin.fail()) {
                std::cout << "Input file name: ";
                std::cin >> file_name;
                fin.open(file_name);
            }
            // task2/case1.cpp
            std::string command = "g++ -g -rdynamic -o output/target ";
            command += file_name;
            command += " -Wl,-Map,output/target.map";
            system(command.data());

            const char *exec_file = "LD_PRELOAD=./task2/libmem.so ./output/target";
            system(exec_file);
            fin.close();

            std::cout << "Input q or Q to quit: ";
            std::string quit_input;
            std::cin >> quit_input;
            while (quit_input != "q" && quit_input != "Q") {
                std::cout << "Input q or Q to quit: ";
                std::cin >> quit_input;
            }
        } else if (type == "q" || type == "Q") {
            return (0);
        }
    }
}

