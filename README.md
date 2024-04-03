## Runing this model in the Finisterrae supercomputer

This folder contains code to run the CSER squiggle.c model on the Finisterrae supercomputer, with the help of Jorge Sierra. Partly this is because the question of whether the result changes as we move upwards from 1M samples to 1B and 1T samples is potentially relevant to some hefty donation decisions. But also, it's just intrinsically a fun project. This readme will later transition into a blogpost.

## Steps done

- [x] Pull the whole model into a single file
- [x] Reword the model to be independent from gcc (and in particular, stop having nested functions)
- [x] Pull the code into the supercomputer
- [x] Architect the deployment
- [x] Find compiler flags for openmp for its compiler
- [x] Write draft paper
- Dudas: 
    - [x] Qué comandos estás utilizando para ejecutar esto? Los puedes añadir al makefile?
    - Estilo: 
      - me gusta que las funciones empiecen con un verbo. 
      - result es demasiado genérico, lo he cambiado a chunk
        - de hecho, tienes various tipos de results (samples, chunks)
      - myid, myblah es un mal patrón
    - Qué es npes?
    - [x] To do: change combination of means
    - [x] To do: get one sample for initial combination
- [x] Have a single sample_mpi function
  - Draw sample and chunkify at the same time, rather than in two loops
- [x] Histogram code.
    - Some options:
    - As a linked list? Easier to expand, but O(n) access time 
    - As an array with some structure <= this would be the sophisticated way to go about this
      - Much more annoying to expand
      - Constant time access
    - [x] Make histogram with constant ticks but very wide and later expand?
      - Give error if it exceeds those bounds?
      - Much simpler to implement
      - Then have *outliers* as a linked list?

## Steps that remain

- [ ] Report results, and how they change as the number of samples increases
- [ ] Insult Python
- [ ] Por qué es "mpi.h" en vez de <mpi.h>???
- [ ] What is #pragma omp single ??
- [ ] Improve histogram code

---

Notes on histogram.

