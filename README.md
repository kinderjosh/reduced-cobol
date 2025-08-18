# COBOL

An open-source COBOL compiler that generates native C99 code.

## Installation

Clone the repository and run the makefile:

```console
$ git clone https://github.com/kinderjosh/cobol.git
$ cd cobol
$ make
```

## Usage

```
./cobc <command> [options] <input file>
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

This project is distributed under the [MIT](./LICENSE) license.