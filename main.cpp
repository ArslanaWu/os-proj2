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

            std::string line;
            while (!fin.eof()) {
                getline(fin, line);
                std::cout << line << std::endl;
            }

            fin.close();

            sleep(5);
        } else if (type == "q" || type == "Q") {
            return (0);
        }
    }
}

