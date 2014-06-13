Complete Integer Project
===============================
This project aims to provide a better support for LLVM IR vector operations. Usually,
when a vector is not legal, like `v32i1`, LLVM will either "widen" or "promote element" this vector.
We propose a third way here. An example would be good:

```
%add = add <32 x i1> %a, %b
```

It's equvalent of the following:
```
%add = xor i32 %a, %b
```

And we can achieve this through manipulating the selectionDAG.
The full description of this project can be found [here](http://parabix.costar.sfu.ca/wiki/CompleteInteger).


Low Level Virtual Machine (LLVM)
================================

This directory and its subdirectories contain source code for the Low Level
Virtual Machine, a toolkit for the construction of highly optimized compilers,
optimizers, and runtime environments.

LLVM is open source software. You may freely distribute it under the terms of
the license agreement found in LICENSE.txt.

Please see the documentation provided in docs/ for further
assistance with LLVM, and in particular docs/GettingStarted.rst for getting
started with LLVM and docs/README.txt for an overview of LLVM's
documentation setup.

If you're writing a package for LLVM, see docs/Packaging.rst for our
suggestions.
