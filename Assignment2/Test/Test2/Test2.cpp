// A function returns untainted value if all arguments are untainted

#include <iostream>

using namespace std;

int add_func(int a, int b) {
  return a + b;
}

int main() {
  int x = 0;
  int y = 1;
  int z;
  cin >> z;               //Tainted = {z}
  z = add_func(x, y);     //Tainted = {}
  return 0;               //Tainted = {}
}