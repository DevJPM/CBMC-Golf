# CBMC-Golf

A Repository dedicated to making CBMC fit for "standard" applications

# Usage

Simply:

    cat file-stored-input | cbmc-golf source-file.c

or whatever you want to call the compilation result or the processed source file.  
The tool  is also able to pass-through the `--unwind` parameter to CBMC via the `-u` flag.

Note that `cbmc` needs to be in your `PATH`.

Note that `file-stored-input` must contain exactly one line for each expected declaration and must only contain valid right-hand-side values for C source-file initialisation e.g. `1` or `{1,2,3}`.
# How to build!?

Simple:

    clang++ cbmc-golf.cpp -o cbmc-golf -O2

or the same with `g++` instead of clang.

# Differences to normal CBMC
 - Print using `p("...",...)`, this will automatically be transpiled to `printf` (with a leading newline so it won't get stripped out of the output)
 - call assert using `a(condition)` which will also get transpiled to `assert`
 - You may not define functions that end in a `p` or an `a` or the above rules will be applied.
 - `stdio.h` and `assert.h` are included by default
 - To save bytes `f()` followed by a new-line will be replaced by `int main() {`
 - At the end of the file a `}` will always be inserted
 - Write your "input" above any preprocessor macros and above `f()` and in the order you want your input lines to be read, they must also be terminated by only a newline (no `;`!)
 - Write your input as standard C declarations, e.g. `int Y` or `int arr[]` or `int arr[][2]` and note that the input you will get will be inserted after that declaration on the right hand side of a `=`
 - For every array-input `X[]` there will automatically be `Xl` defined which holds the length of the variable-length array `X` 
