#!/bin/bash
#SBATCH -o samples-%J.out
#SBATCH -e samples-%J.err
#SBATCH -t 06:00:00 #requested time to run the job
#SBATCH -n 2 #(2 MPI processes)
#SBATCH --ntasks-per-node=1 #(1 process per node, so 4 nodes)
#SBATCH --cpus-per-task=32
#SBATCH --mem 40GB
#SBATCH --mail-type=begin #Envía un correo cuando el trabajo inicia
#SBATCH --mail-type=end #Envía un correo cuando el trabajo finaliza
#SBATCH --mail-user=jorgesierra1998@gmail.com #Dirección a la que se envía

make build

srun ./samples
