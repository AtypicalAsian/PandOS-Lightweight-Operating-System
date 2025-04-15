#include "h/localLibumps.h"
#include "h/tconst.h"
#include "h/print.h"

#define MAX_INPUT 100
#define MAX_NUMS 20
#define INT_MAX  2147483647

/* Converts a string to an integer */
int atoiConversion(char *str, int *val) {
	int i, result, isNegative;
    i = 0;
    result = 0;
    isNegative = 0;

    if (str[i] == '-') {
        isNegative = 1;
        i++;
    }

	while (str[i] >= '0' && str[i] <= '9') {
        if (result > (INT_MAX / 10) || (result == (INT_MAX / 10) && (str[i] - '0') > (INT_MAX % 10))) {
            return -1;
        }
		result = result * 10 + (str[i] - '0');
		i++;
	}
	
    *val = isNegative ? (result * (-1)) : result;

	return i;
}

void swap(int *a, int *b) {
    int temp;
    temp = *a;
    *a = *b;
    *b = temp;
}

void heapify(int arr[], int n, int i) {
    int largest, left, right;
    largest = i;
    left = 2 * i + 1;
    right = 2 * i + 2;

    if (left < n && arr[left] > arr[largest]) {
        largest = left;
    }
    
    if (right < n && arr[right] > arr[largest]) {
        largest = right;
    }

    if (largest != i) {
        swap(&arr[i], &arr[largest]);
        heapify(arr, n, largest);
    }
}

void heapsort(int arr[], int n) {
    int i; 
    for (i = n / 2 - 1; i >= 0; i--) {
        heapify(arr, n, i);
    }
    for (i = n - 1; i >= 0; i--) {
        swap(&arr[0], &arr[i]);
        heapify(arr,i,0);
    }
}

void itoaConversion(int val, char *buf) {
    int i, j, temp, isNegative;
    i = 0;
    isNegative = 0;

    if (val == 0) {
        buf[0] = '0';
        buf[1] = EOS;
        return;
    }

    if (val < 0) {
        isNegative = 1;
        val = -val;
    }

    while (val > 0) {
        buf[i] = (val % 10) + '0';
        i++;
        val /= 10;
    }

    if (isNegative) {
        buf[i] = '-';
        i++;
    }

    /* Reverse the string since we store the last digits first */
    for (j = 0; j < i / 2; j++) {
        temp = buf[j];
        buf[j] = buf[i - 1 - j];
        buf[i - 1 - j] = temp;
    }

    buf[i] = EOS;
}

int main() {
    char input[MAX_INPUT];
    int numbers[MAX_NUMS];
    int count, i, pos, num;
    int status;
    count = 0;
    i = 0;
    pos = 0;
    char numBuf[12];

    print(WRITETERMINAL, "Enter up to 20 integers separated by spaces: ");
    
    status = SYSCALL(READTERMINAL, (int) input, 0, 0);
    if (status < 0) {
        print(WRITETERMINAL, "\nError reading input.\n");
        SYSCALL(TERMINATE, 0, 0, 0);
    }

    /* Remove trailing line if there exists */
    if (input[status - 1] == '\n') {
        status --;
    }
    input[status] = EOS;

    while (i < status && count < MAX_NUMS) {
        while (input[i] == ' ') {
            i++;
        }

        if (i >= status) {
            break;
        }

        pos = atoiConversion(&input[i], &num);
        if (pos > 0) {
            numbers[count] = num;
            count++;
            i += pos;
        } else if (pos < 0) {
            print(WRITETERMINAL, "\nWarning: Number too large or invalid. We skip this number.\n");
            i++;
        } else {
            i++;
        }
    }

    if (count == 0) {
        print(WRITETERMINAL, "\nNo valid numbers entered.\n");
        SYSCALL(TERMINATE, 0, 0, 0);
    }

    heapsort(numbers, count);

    print(WRITETERMINAL, "\nThe result of sorted array: ");
    for (i = 0; i < count; i++) {
        itoaConversion(numbers[i], numBuf);
        print(WRITETERMINAL, numBuf);
        print(WRITETERMINAL, " ");
    }

    print(WRITETERMINAL, "\n");

    SYSCALL(TERMINATE, 0, 0, 0);
}