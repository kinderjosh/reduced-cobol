# RED COBOL

RED COBOL is an open-source compiler for a reduced subset of COBOL that generates native C99 code.

## Requirements

- gcc
- make

## Installation

Clone the repository and run the makefile:

```console
$ git clone https://github.com/kinderjosh/red-cobol.git
$ cd red-cobol
$ make
```

## Usage

```
./cobc <command> [options] <input file>
```

| Command | Description |
| --- | --- |
| build | Produce an executable. |
| object | Produce an object file. |
| source | Produce a C file. |
| run | Build and run the executable. |

| Option | Description |
| --- | --- |
| -include ```<header>``` | Include a C header. |
| -l ```<library>``` | Link with a C library. |
| -no-main | Don't add a main function. |
| -o ```<output file>``` | Specify the output filename. |

## License

RED COBOL is distributed under the [MIT](./LICENSE) license.