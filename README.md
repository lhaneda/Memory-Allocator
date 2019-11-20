# Project 3: Memory Allocator

See: https://www.cs.usfca.edu/~mmalensek/cs326/assignments/project-3.html 

This is custom memory allocator which contains best fit, worst fit, and first fit algorithms. It contains functions and their respective thread safe versions which are accessed when the the critical section is being entered. There are functions that replace the %p and %zu in order to convert pointer and size_t items to text outputs. Heap expansion is done in a seperate function from malloc. Included in this repository is a .png file showing the blocks as part of a linkedlist. 

To compile and use the allocator:

```bash
make
LD_PRELOAD=$(pwd)/allocator.so ls /
```

(in this example, the command `ls /` is run with the custom memory allocator instead of the default).


## Graphviz
```
sudo pacman -Sy graphviz
cd tests/viz/
./visualize-mem.bash mem.txt output.png
## Testing
```
To execute the test cases, use `make test`. To pull in updated test cases, run `make testupdate`. You can also run a specific test case instead of all of them:

```
# Run all test cases:
make test

# Run a specific test case:
make test run=4

# Run a few specific test cases (4, 8, and 12 in this case):
make test run='4 8 12'
```

An Interesting Chain of Allocations done when "LD_PRELOAD=$(pwd)/allocator.so ls" is entered
```
allocator.c:400:malloc(): ALLOCATING SIZE 5
allocator.c:317:expand_heap(): ALLOCATED NEW REGION AT 0x7fbb48ea1000
allocator.c:538:free(): FREE request at 0x7fbb48ea1050
allocator.c:509:free_unsafe(): FREE IS CAUSING REGION 0x7fbb48ea1000 TO UNMAP
allocator.c:400:malloc(): ALLOCATING SIZE 120
allocator.c:317:expand_heap(): ALLOCATED NEW REGION AT 0x7fbb48ea1000
allocator.c:400:malloc(): ALLOCATING SIZE 12
allocator.c:400:malloc(): ALLOCATING SIZE 776
allocator.c:400:malloc(): ALLOCATING SIZE 112
allocator.c:400:malloc(): ALLOCATING SIZE 1336
allocator.c:400:malloc(): ALLOCATING SIZE 216
allocator.c:400:malloc(): ALLOCATING SIZE 432
allocator.c:400:malloc(): ALLOCATING SIZE 104
allocator.c:400:malloc(): ALLOCATING SIZE 88
allocator.c:400:malloc(): ALLOCATING SIZE 120
allocator.c:317:expand_heap(): ALLOCATED NEW REGION AT 0x7fbb48e6f000
```
