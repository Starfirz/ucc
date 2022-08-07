#include <netdb.h>
#include <netinet/tcp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    int size = 1, count = 11;
    int i, len;
    char *servername = argv[1];
    char *buf;
    double delta;
    struct timespec start, stop;
    int iters = 1000;

    int yes = 1;
    int ret;
    struct sockaddr_storage their_addr;
    socklen_t addr_size;
    struct addrinfo hints;
    struct addrinfo *res;
    int sockfd, new_fd;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((ret = getaddrinfo(servername, "12345", &hints, &res)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        return 1;
    }

    if (!servername)
    {
        
        if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1)
        {
            perror("socket");
            return 1;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
        {
            perror("setsockopt");
            return 1;
        }

        if (bind(sockfd, res->ai_addr, res->ai_addrlen) == -1)
        {
            perror("bind");
            return 1;
        }

        if (listen(sockfd, 1) == -1)
        {
            perror("listen");
            return 1;
        }

        addr_size = sizeof their_addr;

        if ((new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size)) == -1)
        {
            perror("accept");
            return 1;
        }
        
        // Warm up
        buf = malloc(1024);
        for (i = 0; i < iters; i++)
        {
            if (read(new_fd, buf, size) == -1)
            {
                perror("read");
                return 1;
            }
        }   
        free(buf);

        // Actual benchmarking loop
        for (i = 0; i < count; i++)
        {
            buf = malloc(size);

            for (int j = 0; j < (iters * size); )
            {
                len = read(new_fd, buf, size);
                if (len == -1)
                {
                    perror("read");
                    return 1;
                }

                j += len;
            }
            
            free(buf);

            size *= 2;
        }
    }
    else
    {
        sleep(1);

        if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1)
        {
            perror("socket");
            return 1;
        }

        if (connect(sockfd, res->ai_addr, res->ai_addrlen) == -1)
        {
            perror("connect");
            return 1;
        }

        if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(int)) == -1)
        {
            perror("setsockopt");
            return 1;
        }

        // Warm up
        buf = malloc(1024);
        for (i = 0; i < iters; i++)
        {
            if (write(sockfd, buf, size) != size)
            {
                perror("write");
                return 1;
            }
        }
        free(buf);

        // Actual benchmarking loop
        for (i = 0; i < count; i++)
        {
            buf = malloc(size);

            if (clock_gettime(CLOCK_MONOTONIC, &start) == -1)
            {
                perror("clock_gettime");
                return 1;
            }

            for (int j = 0; j < iters; j++)
            {
                if (write(sockfd, buf, size) != size)
                {
                    perror("write");
                    return 1;
                }
            }

            if (clock_gettime(CLOCK_MONOTONIC, &stop) == -1)
            {
                perror("clock_gettime");
                return 1;
            }

            delta = (double)((stop.tv_sec - start.tv_sec) * 1000000 + (stop.tv_nsec - start.tv_nsec) / 1000);

            printf("%d\t%.2f\tMb/s\n", size, (size * 8 * 1000) / delta);

            size *= 2;

            free(buf);
        }
    }

    return 0;
}
