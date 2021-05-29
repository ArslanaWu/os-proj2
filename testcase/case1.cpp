#include <iostream>

using namespace std;

int main() {
    int *ptr = new int;
    int *ptr1 = new int;
    int *ptr2 = new int;
    int *ptr3 = new int;
    int *ptr4 = new int;
    // cout << 1 / 0;
    delete ptr;
    return 0;
}
