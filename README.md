# ff

`ff` is a small C compiler written as a single C file. The goal is not to implement all of C, but to keep enough of a simple C-like subset that the compiler can compile its own source. Different constructs that can make code cleaner are not used in the code to to keep the subset of features supported relatively small. For example, there is no support for switch statements as the 

The compiler reads C source from standard input and writes ARM64 assembly to standard output. This does not implement the assembler nor the linker.

```sh
./a.out < ff.c > out.s
cc out.s -o out
```

After that, `out` is the compiler produced by `ff` itself.

## Examples

A minimal input program:

```c
int main(void)
{
    return 42;
}
```

Compile and run it with the host-built compiler:

```sh
gcc ff.c
./a.out < tests/002_return_constant.c > out.s
cc out.s -o out
./out
```

A slightly larger example using locals and control flow:

```c
int main(void)
{
    int i = 0;
    int sum = 0;
    while (i < 5) {
        sum = sum + i;
        i = i + 1;
    }
    return sum;
}
```

Pointers, arrays, structs, calls, strings, and simple typedefs are also covered by the test cases under `tests/`.

## Supported C Subset

`ff` supports a small practical subset of C: `char`, `int`, `long`, `void`, pointers, arrays, structs, typedefs, enums, globals, locals, functions, calls, string and character literals, `sizeof`, arithmetic, comparisons, logical operators, assignment, `if`, `while`, `for`, blocks, and `return`.

It intentionally does not support all of C. Notable unsupported features include `switch`, `break`, `continue`, `++`, `--`, compound assignment, full preprocessing, full initializer semantics, and variadic-call ABI handling.

## Development

Run the test suite with:

```sh
make test
```

Run the self-compilation path with:

```sh
make run
```
