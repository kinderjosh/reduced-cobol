# COBOL

This repository contains a transpiler for COBOL that I started writing while drunk. It transpiles to C99 and is written from scratch.

## Installation

Clone the repository and run the makefile:

```console
$ git clone https://github.com/kinderjosh/cobol.git
$ cd cobol
$ make
```

## Usage

```
./cbl <command> [options] <input file>
```

| Command | Description |
| --- | --- |
| build | Produce an executable. |
| source | Produce a C file. |
| run | Build and run the executable. |

| Option | Description |
| --- | --- |
| -o ```<output file>``` | Specify the output filename. |

## License

The COBOL transpiler is distributed under the [MIT](./LICENSE) license.