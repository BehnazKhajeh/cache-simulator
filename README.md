🔹 Phase 1: Implementing the Cache Simulator
In the first phase, you will be guided through developing a Level-1 cache simulator in the C programming language. This simulator is trace-driven, meaning it receives a sequence of memory access traces as input and simulates cache behavior accordingly.

The trace represents memory references collected by running benchmark programs on a sample system. For each reference in the trace (e.g., instruction fetch or data access), your simulator will perform cache operations and record the results.

The simulator also takes in cache configuration parameters, including:

Total cache size (in bytes)

Block size (in bytes)

Whether the cache is unified or split (instruction/data)

Degree of associativity

Write policy on hits (write-back or write-through)

Write policy on misses (write-allocate or no-write-allocate)

🔹 Phase 2: Performance Evaluation
In the second phase, you will use the simulator to explore how changing cache parameters affects performance. By modifying inputs such as associativity or block size, you will analyze the behavior of different cache configurations.

