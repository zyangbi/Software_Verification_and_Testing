// A function that contains loop

#include <iostream>

using namespace std;

int main() {
  int x = 0;
  cin >> x;               // Tainted = {x}
  int y;

  for (int i = 0; i < y; i++) {
    y = y * 2;            // Tainted = {x} -> {x, y}
    y = x;                // Tainted = {x, y}
  }

  return 0;               // Tainted = {x, y}
}