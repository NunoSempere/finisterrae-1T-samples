## Done

- [x] Pull the whole model into a single file
- [x] Reword the model to be independent from gcc (and in particular, stop having nested functions)
- [x] Pull the code into the supercomputer
- [x] Architect the deployment
- [x] Find compiler flags for openmp for its compiler
- [x] Write draft paper
- [x] Dudas: 
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
      - Then have *outliers* as a linked list
- [x] Deal with output?
  - [x] Print to file, every n iterations
  - Finisterrae sends a file
- [x] Save outliers? => use struct instead of file
- [x] Expand outliers struct if overflows 
- [?] Expand bin width to fix overlow
- [x] Choose histogram & stats & num samples parameters 
- [x] Is there something wrong with the aggregator? It seems like the n increases much slower?
- [x] Por qué es "mpi.h" en vez de <mpi.h>???
- [x] What is #pragma omp single and why is it used together with #pragma omp parallel?
- [x] Improve histogram code
  - [x] Complete or rewamp outliers code

## Steps that remain

- [ ] Report results, and how they change as the number of samples increases
  - [ ] Add draft paper.
- [ ] Find a better way to represent both long tails and more granularity between 0 and 1. E.g., a log scale?

## Discarded step

- [x] Integrate with original squiggle.c repository
