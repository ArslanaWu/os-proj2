#include "top/top_test.cpp"

int main() {
    std::string type;

    while (true) {
        printf("\033c"); // clear old
        std::cout << "Input u/U to monitor memory usage, "
                  << "d/D to detect memory dynamics and leaks, "
                  << "q/Q to quit: ";
        std::cin >> type;

        if (type == "u" || type == "U") {
            top_main();
        } else if (type == "d" || type == "D") {
            std::string file_name = "";
            FILE *file;

            std::cout << "Input file name: ";
            std::cin >> file_name;
            while (!(file = fopen(file_name.c_str(), "r"))) {
                std::cout << "Input file name: ";
                std::cin >> file_name;
            }
            fclose(file);
            std::cout << "!";
        } else if (type == "q" || type == "Q") {
            return (0);
        }
    }

}

