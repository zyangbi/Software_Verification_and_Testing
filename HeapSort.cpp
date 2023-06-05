7. void heapify(int arr[], int n, int i)
8. {
9. 	// int largest = i; // Initialize largest as root
10. 	int largest; // BUG!!!
11. 	int l = 2 * i + 1; // left = 2*i + 1
12. 	int r = 2 * i + 2; // right = 2*i + 2
13. 
14. 	// If left child is larger than root
15. 	if (l < n && arr[l] > arr[largest])
16. 		largest = l;
17. 
18. 	r = largest;
19. 
20. 	// If right child is larger than largest so far
21. 	if (r < n && arr[r] > arr[largest])
22. 		largest = r;
23. 
24. 	// If largest is not root
25. 	if (largest != i) {
26. 		swap(arr[i], arr[largest]);
27. 
28. 		// Recursively heapify the affected sub-tree
29. 		heapify(arr, n, largest);
30. 	}
31. }
