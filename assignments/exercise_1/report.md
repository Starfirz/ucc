# Exercise_1: Benchmarking Throughput
## Implementation

In order to use this application to measure throughput between two machines, running the command `./server` to start the server at one node, then running the command `./client <server-ip>` to start the client at another node.

According to our implementation, under each message size, the client will send 1000 messages to the server. When the server accepts all the messages, the client will stop the timer and calculate the throughput corresponding to each message size. We use Mb /s as the unit of throughput.

------
## Performance results

![image-20220805155846235](C:\Users\BurningOrange\AppData\Roaming\Typora\typora-user-images\image-20220805155846235.png)