#include <iostream>

using namespace std;



void heapify(int arr[], int n, int i)
{
	// int largest = i; // Initialize largest as root
 	int largest; // BUG!!!
 	int l = 2 * i + 1; // left = 2*i + 1
 	int r = 2 * i + 2; // right = 2*i + 2
 
 	// If left child is larger than root
 	if (l < n && arr[l] > arr[largest])
 		largest = l;
 
 	r = largest;
 
 	// If right child is larger than largest so far
 	if (r < n && arr[r] > arr[largest])
 		largest = r;
 
 	// If largest is not root
 	if (largest != i) {
 		swap(arr[i], arr[largest]);
 
 		// Recursively heapify the affected sub-tree
 		heapify(arr, n, largest);
 	}
 }
