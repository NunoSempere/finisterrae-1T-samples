Sampling 10e9 points from a Fermi estimate in the Finisterrae supercomputer
===========================================================================

## Index

- Abstract
  - It is, in some sense, overkill. The model is informal, and degrees of precision are, in some sense, not needed. At the same time, we just felt curious. Seemed like an interesting programming and engineering challenge.
- Architecture of the distributional code. Discussed in the squiggle.c repository.
  - What is squiggle
    - Generally, a set of ergonomic idioms designed to express judgmental estimates. They grew from foretold.io, forecasting platform that hosted experiments and that seeked to advance the state of the art.
    - Specifically, a typescript (previously OCaml) implementation
  - What is squiggle.c 
    - Optimized for interpretability and speed
    - Go or Rust may have been better choices
- Programming challenges
  - Choice of randomness function
  - Single-core parallelism
  - Multi-core parallelism
- Comparison with other languages.
  - Dependence on language-specific idioms
  - Why not assembly
  - Why not CUDA
- Sensitivity to the number of samples 
- Conclusion
