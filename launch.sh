#!/bin/bash
#SBATCH -o samples-%J.out
#SBATCH -e samples-%J.err
# #SBATCH -C clk
#SBATCH --time=0-00:15:00 #requested time to run the job
#SBATCH -n 2 #(2 MPI processes)
#SBATCH --ntasks-per-node=1 #(1 process per node, so 4 nodes)
#SBATCH --cpus-per-task=8
#SBATCH --mem 10GB
#190GB
module purge
module load intel impi
make build

srun ./samples