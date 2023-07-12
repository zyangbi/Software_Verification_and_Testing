// Reassignments in conditional branches cannot untaint tainted variables.

#include <iostream>

using namespace std;

int main() {
  int x = 0;
  int y = 1;
  cin >> x;               //Tainted = {x}
  if (y > 0) {
    x = 0;                //Tainted = {x}
  } else {
    x = 1;                //Tainted = {x}
  }
  return 0;               //Tainted = {x}
}