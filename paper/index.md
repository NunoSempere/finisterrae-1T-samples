---
title: "Drawing 10e12 samples from a judgmental estimation model"
author: Jorge Sierra, Nuño Sempere
date: \today
urlcolor: blue
abstract: "To draw 10e12 samples you need two things: a fast language, and a supercomputer."
---

## Background and motivation

Estimators in the tradition of Fermi or Tetlock sometimes create models that seek to capture their subjective credences—as opposed to, for instance, fitting a model to an underlying set of samples. One way one might go about this is by expressing a distribution using the set of idioms that grew from the foretold.io experimental forecasting platform into [Squiggle](https://www.squiggle-language.com/)—now a programming language for intuitive probabilitistic estimation. The original iteration of squiggle has been ported to Python ([Squiggle.py](https://github.com/rethinkpriorities/squigglepy)), to C ([Squiggle.c](https://git.nunosempere.com/personal/squiggle.c)), or to [go](https://git.nunosempere.com/NunoSempere/fermi).

However, those idioms favour relying on Monte Carlo estimation, that is, approximating a model by drawing samples rather than specifying the model exactly. This approach sometimes has difficulty modelling long-tail behaviour. To explore those limits, and also just for the sake of the technical challenge, we describe how to draw 10e12 samples (an American trillion) from a judgmental model estimating the altruistic impact of [Sentinel](https://sentinel-team.org/), a foresight and emergency response team set up by one of the authors, in basis points of existential/catastrophic risk[^cat] averted per million dollars. We show how summary statistics change as we draw more samples. We conclude discussing limitations and practical applications.

[^cat]: with a conversion factor of existential risk being 100x worse than catastrophic risk.

## 1. How to draw 10e9 samples

Our [time to botec](https://github.com/NunoSempere/time-to-botec/) repository compares how fast drawing 10e6 samples from a simple back-of-the-envelope calculation runs in different languages. The comparison depends on our familiarity with the different languages: as we are more familiar with a language, we grow capable of writing better & faster code in it. Nonetheless, speeds to get 10e6 samples in one author's computer are as follows:

| Language                    | Time      | 
|-----------------------------|-----------|
| C                           | 6.20ms    | 
| squiggle.c                  | 7.20ms    | 
| go                          | 21.20ms   | 
| Nim                         | 41.10ms   | 
| Lua (LuaJIT)                | 68.80ms   | 
| Python (numpy)              | 118ms     | 
| OCaml (flambda)             | 185ms     | 
| Squiggle (bun)              | 384ms     | 
| Javascript (node)           | 395ms     | 
| SquigglePy (v0.27)          | 1,542ms   | 
| R (3.6.1)                   | 4,494ms   | 
| Python 3.9                  | 11,909ms  | 
| Gavin Howard's bc           | 16,170ms  | 

Readers might have expected this list to include languages such as Rust or zig. FORTRAN, Lisp, Haskell, Java, C#, C++, or Julia are also missing. So are faster options, like GPU programming. This is because we are not very familiar with those languages, or because in our attempts to use them we found them too "clunky". 

Still, the above table shows that the time it takes to run a Fermi model varies 3 to 4 orders of magnitude depending on the language choice (and skill at using it), and so language choice will be an important choice. 

Once we have chosen a fast language like C, we can deploy some standard practices, like inlining functions, passing pointers not values, or using [compiler flags](https://github.com/NunoSempere/time-to-botec/blob/master/squiggle.c/makefile#L3) for greater speed. The difference between an [initial approach](https://github.com/NunoSempere/time-to-botec/blob/f0493f695552b3b76c34d5e18f2f3f14ac8e4bd9/C/samples/samples.c) and a [final version with more care](https://github.com/NunoSempere/time-to-botec/blob/master/C/samples.c) can be notable.

A memorable moment was switching from antiquated libraries to reading the literature and simply baking in our own randomness primitives, mostly following the work of Marsaglia. Back in our javascript days, stlib.io had needless complexity because of the need to support incompatible types of javascript models. 

This is somewhat of a judgment call. Languages such as Rust, zig, or even go have some well-optimized mathematical primitives, and pulling them in might be the better choice.

#### 1.2.2. Use OpenMP

#### 1.2.3. Use MPI

Compile to the native architecture. Profile. Compiler flags.

Perhaps concession was defining a mixture
One of the few concessions was defining a function to mix several other distributions. This was worse than doing something like: 

```
double p = sample_uniform(0,1, seed);
if(p < p1){
  return sample_dist_1(seed);
} else if (p<p1+p2){
  return sample_dist_2(seed);
} else {
  return sample_dist_3(seed);
}

```

but much more convenient

Go has almost none of these tricks. 

### 1.3. Introduce parallelism with OpenMP 

Introducing parallelism with OpenMP was fairly straightforward. 

alignment

### 1.4. Run the code on a supercomputer with MPI

Talk about challenges here.

Compiling on icc compiler. Requires not using useful gcc extensions, like nested functions. Reader can find recipes in the makefile. 

### 1.5. Run the code for a long time

Finally, we can get around 3x more samples by running the code for 18 hours instead of 6 hours, getting to 10T samples. [don't do this yet]. Increasing time quickly becomes untenable, however.

## 2. Results as samples increase 

### 2.1. Mean and standard deviation

### 2.2. Minimum and maximum

## 3. An abuse of the underlying PRNG primitives?

Do the underlying pseudo-random number generator primitives we are using stand up to scaling to 1T samples? 
If you can draw 1T samples *at all*, you can draw 100K, 1M, 10M samples very fast.

## Conclusion

Rethink Priorities' models puny struggled (https://forum.effectivealtruism.org/posts/pniDWyjc9vY5sjGre/rethink-priorities-cross-cause-cost-effectiveness-model) to reach more than 150k samples. Eventually, with some optimizations, they moved to the billions. Much more is possible with a focus on speed. 


---

The virtue of fast Monte Carlo primitives: Drawing 1T samples and a REPL for rapid fermi iteration
