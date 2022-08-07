# Exercise_4: UCC
## Part 1

**Run MPI_igatherv on host** ("2 1 1")

![image-20220804144332364](C:\Users\BurningOrange\AppData\Roaming\Typora\typora-user-images\image-20220804144332364.png)



**Run MPI_igatherv offload on DPU** ("2 1 1")

![image-20220804002939910](C:\Users\BurningOrange\AppData\Roaming\Typora\typora-user-images\image-20220804002939910.png)

**Run MPI_igatherv offload on DPU** ("8 4 1")

![image-20220804003007691](C:\Users\BurningOrange\AppData\Roaming\Typora\typora-user-images\image-20220804003007691.png)

**Run MPI_igatherv offload on DPU** ("8 4 2")

![image-20220804003113953](C:\Users\BurningOrange\AppData\Roaming\Typora\typora-user-images\image-20220804003113953.png)

**Run MPI_igatherv offload on DPU** ("8 4 4")

![image-20220804002052219](C:\Users\BurningOrange\AppData\Roaming\Typora\typora-user-images\image-20220804002052219.png)

As the number of host processes increased from 2 to 8, the overall latency increased significantly, but the overlap rate improved, exceeding 96% at all message sizes. When the service process on each DPU increases, the latency gradually decreases and the overlap rate slightly decreases too, but it still remains above 95%.



**Run MPI_gatherv on host** ("2 1 1")

![image-20220804002250218](C:\Users\BurningOrange\AppData\Roaming\Typora\typora-user-images\image-20220804002250218.png)

**Run MPI_gatherv offload on DPU** ("2 1 1")

![image-20220804004114824](C:\Users\BurningOrange\AppData\Roaming\Typora\typora-user-images\image-20220804004114824.png)

Using the same combination, running MPI_gatherv on the DPU, we can find that the average latency is significantly higher than when running on the host, but it does not change much as the message size increases, and at the maximum message size, the average latency is even lower than when running on the host.



**hybridized  algorithms**

![image-20220804002834549](C:\Users\BurningOrange\AppData\Roaming\Typora\typora-user-images\image-20220804002834549.png)


------
## Part 2

**Run MPI_bcast on host** ("2 1 1")

![image-20220804003254678](C:\Users\BurningOrange\AppData\Roaming\Typora\typora-user-images\image-20220804003254678.png)

**Run MPI_bcast on host** ("8 4 1")

![image-20220804161026813](C:\Users\BurningOrange\AppData\Roaming\Typora\typora-user-images\image-20220804161026813.png)

![image-20220805001518312](C:\Users\BurningOrange\AppData\Roaming\Typora\typora-user-images\image-20220805001518312.png)

As we can see from the chart above, when the message size is less than 16384, the average latency of the two combinations is not much different, and the latency difference is more obvious when size is greater than 16384. When the number of host processes is 8, the latency grows much faster than the number of host processes at 2.



**Run MPI_ibcast on host** ("2 1 1")

![image-20220804003411872](C:\Users\BurningOrange\AppData\Roaming\Typora\typora-user-images\image-20220804003411872.png)

**Run MPI_ibcast on host** ("8 4 1")

![image-20220804161249959](C:\Users\BurningOrange\AppData\Roaming\Typora\typora-user-images\image-20220804161249959.png)



**Run MPI_Ibcast (HPCA version) on host ** (“2 1 1”)

![image-20220805145901512](C:\Users\BurningOrange\AppData\Roaming\Typora\typora-user-images\image-20220805145901512.png)

**Run MPI_Ibcast (HPCA version) on host ** (“8 4 1”)

![image-20220805152020209](C:\Users\BurningOrange\AppData\Roaming\Typora\typora-user-images\image-20220805152020209.png)

------
## part 3