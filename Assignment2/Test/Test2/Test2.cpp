// A function returns untainted value if all arguments are untainted

#include <iostream>

using namespace std;

int add_func(int a, int b) {
  return a + b;
}

int multi_func(int a, int b, int c) {
  return a * b * c;
}

int main() {
  int x = 0;
  int y = 1;
  int z;
  cin >> z;               //Tainted = {z}
  z = add_func(x, y);     //Tainted = {}
  int a;
  cin >> a;
  a = multi_func(x, y, z);
  return 0;               //Tainted = {}
}