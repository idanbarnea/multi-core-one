The attack works!

The goal was to perform a 4K aliasing in the load-store buffer.
We assumed that
1. when a store is performed in the mid-core, the cpu keeps the value in a buffer, for a load-store forwarding
2. when a load comes after the store, the cpu checks if they have the same address, and if so it forwards the store value to the load.
3. The comparison is first done with virtual addresses, and if it they are true the cpu *speculativly* forwards the value to the load.
3.1. Later, the cpu checks if the both the physical addresses are the same, and if not it discards the load (and probably re-issue it)

The main assumtion here is that the comparison is first done on VA and not PA.

We use the same mechanism as in Spectre. We hide the forbidden access with a brach misprediction.
First, we define a secret byte that when we are trying to access it for the array, we will access its address + 4K. Meaning that if we managed somehow to accessed it, we managed to do a 4K aliasing.
We trained the branch predictor to predict that is_mal < array1_size. and in these times store_mal_ptr is stored_ptr, so array2 will access store * jump_size.
But, when we want to do a 4K aliasing, we set to be is_mal > array1_size, and store_mal_ptr = stored_ptr + 4K, so the misprediction will lead to
loaded_value = *stored_ptr_mal! and we access a garbage. But, to show that we didn't took garbage, we access this place in the array array2.
The last phase, the check for the results, is very similar to the spectre file we recieved.

Note that the program works most of time on my Intel-i7 computer after disabling some Windows Defender functionality. But, one may run it several times until it gets the results.