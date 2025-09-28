//
// Created by yh on 2/28/25.
//
#include <iostream>

class Point {
    int x{1}, y{1};

public:
    int getX()  {
        std::cout << "getX\n";
        return x;
    };

    const int getX() const {
        std::cout << "const getX\n";
        return x;
    };

    int getY() const {
        return y;
    };
};

int main() {
    Point p;
    const Point cp;

    std::cout << p.getX() << "\n";
    std::cout << p.getY() << "\n";


    std::cout << cp.getX() << "\n";
    std::cout << cp.getY() << "\n";
}