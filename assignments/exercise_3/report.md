# Exercise_3: UCX Ping-Pong Latency Benchmark
## Implementation

This is a simple message rate / bandwidth benchmark using UCX

Prior to compiling or running, load the following modules:

`module load gcc hpcx`

To compile:

`make`

To run with two nodes on the Thor system:

`mpirun -np 2 --map-by node ./ucx_pingpong`

------
## Performance results

![image-20220807152224849](C:\Users\BurningOrange\AppData\Roaming\Typora\typora-user-images\image-20220807152224849.png)
