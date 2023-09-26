#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define SIZE 12

int verify_password(char* password, int offsetA) {
	int correct = 1;
	int offsetB = 2;
	char passwordBuffer[SIZE];
	const char actualPwd[] = "realpassword";
	
	for (int i=0; i < SIZE; i++) {
	  passwordBuffer[i] = password[i];
	}
	
	for (int i=0; i < SIZE; i++) {
	  char pwdChar = passwordBuffer[i] - offsetA + offsetB;
	  assert(pwdChar >= 'a' && pwdChar <= 'z');  
	  if (pwdChar != actualPwd[i]) {
	    correct = 0;
	  }
	}

	return correct;
}

int main(int argc,char **argv) {
    int offsetA = 2;
    char *str = argv[1];
    int len = strlen(str);

    // Input doesn't exceed 16 characters
    if (len > 16) {
        // printf("Longer than 16");
        return 0;
    }

    // Restrict state space to only check for a-z
    int valid = 1;
    for (int i = 0; i < len; ++i) {
        if (str[i] < 'a' || str[i] > 'z') {
            valid = 0;
            break;
        }
    }
    if (!valid) {
        // printf("no in a to z\n");
        return 0;
    }

    int check = verify_password(str, offsetA);
    if (check) {
        // printf("Password matched!\n");
    } else {
        // printf("Password did not match\n");
    }

    return 0;

}