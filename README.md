## Runing this model in the Finisterrae supercomputer

This folder contains code to run the CSER squiggle.c model on the Finisterrae supercomputer, with the help of Jorge Sierra. Partly this is because the question of whether the result changes as we move upwards from 1M samples to 1B and 1T samples is potentially relevant to some hefty donation decisions. But also, it's just intrinsically a fun project. This readme will later transition into a blogpost.

For now, some work has already been done:

- [x] Pull the whole model into a single file
- [x] Reword the model to be independent from gcc (and in particular, stop having nested functions)

But more remains:

- [ ] Pull the code into the supercomputer
- [ ] Architect the deployment
- [ ] Find compiler flags for openmp for its compiler
- [ ] ...
- [ ] Report results, and how they change as the number of samples increases
- [ ] Insult Python
