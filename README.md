This project was originally contained entirely on https://github.com/jeffhammond/HPCInfo/wiki/MARPN...

TODO: Convert from MediaWiki to MarkDown

# What is it?

MARPN stands for MPI with Arbitrary Ranks-Per-Node.  I developed this for Blue Gene/Q but it is, in principle, general purpose.  On most machines, arbitrary ranks-per-node is trivial, but they have to be homogeneous across the machine.  If you find that restrictive, create a Github issue to let me know that you need this feature supported.

# Credit

MARPN was written by Jeff Hammond using MPI-related tools developed by Todd Gamblin.  This project would not have been possible without Todd's help, but Jeff is responsible for supporting it and deserves all the blame for its shortcomings.

# Why would you use this?

You should only use MARPN as a means of last resort.  There are better ways to solve the problem that it solves but these require work.

## Application Rigidity

Your application is rigid and demands that the process count is directly related to the domain decomposition.  The developers of the application are unwilling to use `MPI_Comm_split`and search-and-replace on `MPI_COMM_WORLD` to fix the problem directly.

## CPU Performance

If your application is limited by some aspect of the memory hierarchy and does not use threads, it might be beneficial to only do work on 3 of the 4 hardware threads per core as a compromise between instruction issue rate (which requires >2 threads to saturate) and memory bandwidth.

## Memory

If your application does not use threads and you want to run on the maximum number of hardware threads possible but you cannot live with the memory available when 64 processes are used, you might find that e.g. 48 processes is sufficient.

_Unfortunately, MARPN does not yet address the memory issue, but I know how to implement it and will do so in the future if there is sufficient demand._

A solution to the memory issue is to override `malloc` to allocate memory in another processes address space using PAMI active-messages, which - in conjunction with the `BG_MAPCOMMONHEAP=1` - will allow you to use more memory than exists in one processes address space. This solution is *E-V-I-L* and will make me feel very bad for a long time, so you might have to incentivize me with something that will help numb the pain.

# How does it work?

* `MPI_COMM_WORLD` is intercepted using the MPI profiling interface and substituted for the subcommunicator that contains the desired number of ranks-per-node.
* Inactive processes go to sleep for 48 hours so that they do not consume CPU resources.
* `MPI_Abort` is called instead of `MPI_Finalize` (after a barrier on the active process communicator) because otherwise the sleeping processes would keep the job idling until it timed out.

## FAQ

* Is it topology-aware on Blue Gene/Q?  Yes, the active ranks are selected in a breadth-first manner.
* Why can't you just call `MPI_Finalize` instead of `sleep(48*60*60)`?  Because that will cause the inactive processes to spin in a barrier and create contention for execution resources with the active processes.
* Is there a way to call `MPI_Finalize` properly?  Yes, but it requires programming the wake-up unit and I don't care to do this.
* Does it work with threads?  No, but only because I did not bother to implement `MPI_Init_thread` properly.  If you can use threads, why do you need MARPN?

# How do you use it?

1. Download [[File:Marpn.C]].

2. `mpicxx -c -g -O2 Marpn.C`

3. `ar -r libmarpn.a Marpn.o`

4. Link your applications with <tt>libmarpn.a</tt> at the end.

5. Submit with the environment variable `MARPN_RANKS_PER_NODE` set to the number of ranks-per-node you want (it must be less than or equal to the number available) as well as `BG_COREDUMPDISABLED=1`.  _You have to disable core files due to the design described above._

## Hello, World!

### Source
```
#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

int main(int argc, char ** argv)
{
    int world_size, world_rank;

    MPI_Init(&argc, &argv);

    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    printf("Hello from %d of %d processors\n", world_rank, world_size);

    MPI_Finalize();

    return 0;
}
```

### Build
```
mpicc -g -O2 -Wall  hello.o libmpiarbrpn.a -o hello.x
```

### Submit
```
qsub -t 20 -n 1 --mode=c64 --env MARPN_DEBUG=1:MARPN_RANKS_PER_NODE=48:BG_COREDUMPDISABLED=1 ./hello.x
```

### Output

#### `stdout`
```
Hello from 4 of 48 processors
Hello from 3 of 48 processors
Hello from 5 of 48 processors
Hello from 13 of 48 processors
Hello from 14 of 48 processors
Hello from 12 of 48 processors
Hello from 32 of 48 processors
Hello from 31 of 48 processors
Hello from 30 of 48 processors
Hello from 1 of 48 processors
Hello from 2 of 48 processors
Hello from 0 of 48 processors
Hello from 36 of 48 processors
Hello from 38 of 48 processors
Hello from 37 of 48 processors
Hello from 8 of 48 processors
Hello from 7 of 48 processors
Hello from 6 of 48 processors
Hello from 18 of 48 processors
Hello from 20 of 48 processors
Hello from 19 of 48 processors
Hello from 33 of 48 processors
Hello from 35 of 48 processors
Hello from 34 of 48 processors
Hello from 41 of 48 processors
Hello from 39 of 48 processors
Hello from 40 of 48 processors
Hello from 43 of 48 processors
Hello from 44 of 48 processors
Hello from 42 of 48 processors
Hello from 29 of 48 processors
Hello from 27 of 48 processors
Hello from 28 of 48 processors
Hello from 47 of 48 processors
Hello from 45 of 48 processors
Hello from 46 of 48 processors
Hello from 17 of 48 processors
Hello from 15 of 48 processors
Hello from 16 of 48 processors
Hello from 10 of 48 processors
Hello from 9 of 48 processors
Hello from 11 of 48 processors
Hello from 24 of 48 processors
Hello from 25 of 48 processors
Hello from 26 of 48 processors
Hello from 23 of 48 processors
Hello from 22 of 48 processors
Hello from 21 of 48 processors
```

#### `stderr`
```
rank 14 (core 3, hwtid 2) is included in the new world 
rank 13 (core 3, hwtid 1) is included in the new world 
rank 12 (core 3, hwtid 0) is included in the new world 
rank 15 (core 3, hwtid 3) is excluded from the new world 
rank 48 (core 12, hwtid 0) is included in the new world 
rank 51 (core 12, hwtid 3) is excluded from the new world 
rank 49 (core 12, hwtid 1) is included in the new world 
rank 50 (core 12, hwtid 2) is included in the new world 
rank 23 (core 5, hwtid 3) is excluded from the new world 
rank 20 (core 5, hwtid 0) is included in the new world 
rank 21 (core 5, hwtid 1) is included in the new world 
rank 22 (core 5, hwtid 2) is included in the new world 
rank 39 (core 9, hwtid 3) is excluded from the new world 
rank 36 (core 9, hwtid 0) is included in the new world 
rank 37 (core 9, hwtid 1) is included in the new world 
rank 38 (core 9, hwtid 2) is included in the new world 
rank 41 (core 10, hwtid 1) is included in the new world 
rank 43 (core 10, hwtid 3) is excluded from the new world 
rank 42 (core 10, hwtid 2) is included in the new world 
rank 40 (core 10, hwtid 0) is included in the new world 
rank 61 (core 15, hwtid 1) is included in the new world 
rank 60 (core 15, hwtid 0) is included in the new world 
rank 63 (core 15, hwtid 3) is excluded from the new world 
rank 62 (core 15, hwtid 2) is included in the new world 
rank 1 (core 0, hwtid 1) is included in the new world 
rank 3 (core 0, hwtid 3) is excluded from the new world 
rank 0 (core 0, hwtid 0) is included in the new world 
rank 2 (core 0, hwtid 2) is included in the new world 
rank 28 (core 7, hwtid 0) is included in the new world 
rank 29 (core 7, hwtid 1) is included in the new world 
rank 31 (core 7, hwtid 3) is excluded from the new world 
rank 30 (core 7, hwtid 2) is included in the new world 
rank 33 (core 8, hwtid 1) is included in the new world 
rank 32 (core 8, hwtid 0) is included in the new world 
rank 35 (core 8, hwtid 3) is excluded from the new world 
rank 34 (core 8, hwtid 2) is included in the new world 
rank 11 (core 2, hwtid 3) is excluded from the new world 
rank 10 (core 2, hwtid 2) is included in the new world 
rank 8 (core 2, hwtid 0) is included in the new world 
rank 9 (core 2, hwtid 1) is included in the new world 
rank 52 (core 13, hwtid 0) is included in the new world 
rank 55 (core 13, hwtid 3) is excluded from the new world 
rank 54 (core 13, hwtid 2) is included in the new world 
rank 53 (core 13, hwtid 1) is included in the new world 
rank 44 (core 11, hwtid 0) is included in the new world 
rank 46 (core 11, hwtid 2) is included in the new world 
rank 45 (core 11, hwtid 1) is included in the new world 
rank 47 (core 11, hwtid 3) is excluded from the new world 
rank 18 (core 4, hwtid 2) is included in the new world 
rank 17 (core 4, hwtid 1) is included in the new world 
rank 16 (core 4, hwtid 0) is included in the new world 
rank 19 (core 4, hwtid 3) is excluded from the new world 
rank 24 (core 6, hwtid 0) is included in the new world 
rank 25 (core 6, hwtid 1) is included in the new world 
rank 26 (core 6, hwtid 2) is included in the new world 
rank 27 (core 6, hwtid 3) is excluded from the new world 
rank 56 (core 14, hwtid 0) is included in the new world 
rank 58 (core 14, hwtid 2) is included in the new world 
rank 57 (core 14, hwtid 1) is included in the new world 
rank 59 (core 14, hwtid 3) is excluded from the new world 
rank 6 (core 1, hwtid 2) is included in the new world 
rank 5 (core 1, hwtid 1) is included in the new world 
rank 7 (core 1, hwtid 3) is excluded from the new world 
rank 4 (core 1, hwtid 0) is included in the new world 
rank 7 (core 1, hwtid 3) going to sleep 
rank 19 (core 4, hwtid 3) going to sleep 
rank 43 (core 10, hwtid 3) going to sleep 
rank 3 (core 0, hwtid 3) going to sleep 
rank 51 (core 12, hwtid 3) going to sleep 
rank 11 (core 2, hwtid 3) going to sleep 
rank 27 (core 6, hwtid 3) going to sleep 
rank 47 (core 11, hwtid 3) going to sleep 
rank 55 (core 13, hwtid 3) going to sleep 
rank 59 (core 14, hwtid 3) going to sleep 
rank 39 (core 9, hwtid 3) going to sleep 
rank 63 (core 15, hwtid 3) going to sleep 
rank 23 (core 5, hwtid 3) going to sleep 
rank 15 (core 3, hwtid 3) going to sleep 
rank 35 (core 8, hwtid 3) going to sleep 
rank 31 (core 7, hwtid 3) going to sleep 
Abort(0) on node 36 (rank 36 in comm 1140850688): application called MPI_Abort(MPI_COMM_WORLD, 0) - process 36
Abort(0) on node 38 (rank 38 in comm 1140850688): application called MPI_Abort(MPI_COMM_WORLD, 0) - process 38
Abort(0) on node 37 (rank 37 in comm 1140850688): application called MPI_Abort(MPI_COMM_WORLD, 0) - process 37
Abort(0) on node 52 (rank 52 in comm 1140850688): application called MPI_Abort(MPI_COMM_WORLD, 0) - process 52
Abort(0) on node 53 (rank 53 in comm 1140850688): application called MPI_Abort(MPI_COMM_WORLD, 0) - process 53
Abort(0) on node 54 (rank 54 in comm 1140850688): application called MPI_Abort(MPI_COMM_WORLD, 0) - process 54
Abort(0) on node 58 (rank 58 in comm 1140850688): application called MPI_Abort(MPI_COMM_WORLD, 0) - process 58
Abort(0) on node 57 (rank 57 in comm 1140850688): application called MPI_Abort(MPI_COMM_WORLD, 0) - process 57
Abort(0) on node 56 (rank 56 in comm 1140850688): application called MPI_Abort(MPI_COMM_WORLD, 0) - process 56
Abort(0) on node 4 (rank 4 in comm 1140850688): application called MPI_Abort(MPI_COMM_WORLD, 0) - process 4
Abort(0) on node 5 (rank 5 in comm 1140850688): application called MPI_Abort(MPI_COMM_WORLD, 0) - process 5
```

## LAMMPS

Build LAMMPS as you would normally but add `MPI_LIB += libmpiarbrpn.a` at the end of the MPI section.  Do not compile with the OpenMP module as MARPN does not support `MPI_Init_thread`.

### Output

```
[jhammond@vestalac1 bench]$ for i in `/bin/ls *marpn*ing` ; do grep MPI -H $i ; grep time -H $i ; done

eam2.xl-omp.n128.c32.nomarpn.log.scaling:  using 2 OpenMP thread(s) per MPI task
eam2.xl-omp.n128.c32.nomarpn.log.scaling:  16 by 16 by 16 MPI processor grid
eam2.xl-omp.n128.c32.nomarpn.log.scaling:Loop time of 21.9953 on 8192 procs (4096 MPI x 2 OpenMP) for 100 steps with 32000000 atoms
eam2.xl-omp.n128.c32.nomarpn.log.scaling:timestep	0.005
eam2.xl-omp.n128.c32.nomarpn.log.scaling:Loop time of 21.9953 on 8192 procs (4096 MPI x 2 OpenMP) for 100 steps with 32000000 atoms
eam2.xl-omp.n128.c32.nomarpn.log.scaling:Pair  time (%) = 18.6212 (84.6598)
eam2.xl-omp.n128.c32.nomarpn.log.scaling:Neigh time (%) = 2.44077 (11.0968)
eam2.xl-omp.n128.c32.nomarpn.log.scaling:Comm  time (%) = 0.662425 (3.01166)
eam2.xl-omp.n128.c32.nomarpn.log.scaling:Outpt time (%) = 0.00405421 (0.0184321)
eam2.xl-omp.n128.c32.nomarpn.log.scaling:Other time (%) = 0.266885 (1.21337)

eam2.xl-omp.n128.c32.f24.marpn.log.scaling:  8 by 16 by 16 MPI processor grid
eam2.xl-omp.n128.c32.f24.marpn.log.scaling:timestep	0.005
eam2.xl-omp.n128.c32.f24.marpn.log.scaling:Loop time of 34.1208 on 2048 procs for 100 steps with 32000000 atoms
eam2.xl-omp.n128.c32.f24.marpn.log.scaling:Pair  time (%) = 29.0107 (85.0235)
eam2.xl-omp.n128.c32.f24.marpn.log.scaling:Neigh time (%) = 3.77973 (11.0775)
eam2.xl-omp.n128.c32.f24.marpn.log.scaling:Comm  time (%) = 0.884697 (2.59284)
eam2.xl-omp.n128.c32.f24.marpn.log.scaling:Outpt time (%) = 0.00462089 (0.0135427)
eam2.xl-omp.n128.c32.f24.marpn.log.scaling:Other time (%) = 0.441052 (1.29262)

eam2.xl-omp.n128.c64.f32.marpn.log.scaling:  16 by 16 by 16 MPI processor grid
eam2.xl-omp.n128.c64.f32.marpn.log.scaling:timestep	0.005
eam2.xl-omp.n128.c64.f32.marpn.log.scaling:Loop time of 21.5919 on 4096 procs for 100 steps with 32000000 atoms
eam2.xl-omp.n128.c64.f32.marpn.log.scaling:Pair  time (%) = 18.2162 (84.3659)
eam2.xl-omp.n128.c64.f32.marpn.log.scaling:Neigh time (%) = 2.40694 (11.1474)
eam2.xl-omp.n128.c64.f32.marpn.log.scaling:Comm  time (%) = 0.686816 (3.18089)
eam2.xl-omp.n128.c64.f32.marpn.log.scaling:Outpt time (%) = 0.0039327 (0.0182137)
eam2.xl-omp.n128.c64.f32.marpn.log.scaling:Other time (%) = 0.278013 (1.28758)

eam2.xl-omp.n128.c64.f37.marpn.log.scaling:  16 by 8 by 37 MPI processor grid
eam2.xl-omp.n128.c64.f37.marpn.log.scaling:timestep	0.005
eam2.xl-omp.n128.c64.f37.marpn.log.scaling:Loop time of 21.3751 on 4736 procs for 100 steps with 32000000 atoms
eam2.xl-omp.n128.c64.f37.marpn.log.scaling:Pair  time (%) = 16.9574 (79.3325)
eam2.xl-omp.n128.c64.f37.marpn.log.scaling:Neigh time (%) = 2.09132 (9.78387)
eam2.xl-omp.n128.c64.f37.marpn.log.scaling:Comm  time (%) = 1.48095 (6.92837)
eam2.xl-omp.n128.c64.f37.marpn.log.scaling:Outpt time (%) = 0.0429694 (0.201025)
eam2.xl-omp.n128.c64.f37.marpn.log.scaling:Other time (%) = 0.802464 (3.75419)

eam2.xl-omp.n128.c64.f47.marpn.log.scaling:  16 by 8 by 47 MPI processor grid
eam2.xl-omp.n128.c64.f47.marpn.log.scaling:timestep	0.005
eam2.xl-omp.n128.c64.f47.marpn.log.scaling:Loop time of 17.7326 on 6016 procs for 100 steps with 32000000 atoms
eam2.xl-omp.n128.c64.f47.marpn.log.scaling:Pair  time (%) = 14.2518 (80.371)
eam2.xl-omp.n128.c64.f47.marpn.log.scaling:Neigh time (%) = 1.79042 (10.0968)
eam2.xl-omp.n128.c64.f47.marpn.log.scaling:Comm  time (%) = 1.30428 (7.35526)
eam2.xl-omp.n128.c64.f47.marpn.log.scaling:Outpt time (%) = 0.0194274 (0.109558)
eam2.xl-omp.n128.c64.f47.marpn.log.scaling:Other time (%) = 0.366612 (2.06745)

eam2.xl-omp.n128.c64.f48.marpn.log.scaling:  16 by 16 by 24 MPI processor grid
eam2.xl-omp.n128.c64.f48.marpn.log.scaling:timestep	0.005
eam2.xl-omp.n128.c64.f48.marpn.log.scaling:Loop time of 16.8048 on 6144 procs for 100 steps with 32000000 atoms
eam2.xl-omp.n128.c64.f48.marpn.log.scaling:Pair  time (%) = 13.8626 (82.492)
eam2.xl-omp.n128.c64.f48.marpn.log.scaling:Neigh time (%) = 1.77081 (10.5376)
eam2.xl-omp.n128.c64.f48.marpn.log.scaling:Comm  time (%) = 0.85384 (5.08094)
eam2.xl-omp.n128.c64.f48.marpn.log.scaling:Outpt time (%) = 0.016007 (0.0952529)
eam2.xl-omp.n128.c64.f48.marpn.log.scaling:Other time (%) = 0.301523 (1.79427)
```
