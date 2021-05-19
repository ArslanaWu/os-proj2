#include <iostream>

using namespace std;

int main() {
    int *ptr = new int;
    cout << 1 / 0;
    delete ptr;
    return 0;
}

