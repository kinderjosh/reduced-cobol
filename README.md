# Reduced COBOL

Reduced COBOL is an open-source compiler for a smaller subset of COBOL that generates native C99 code for Windows and Unix-like operating systems.

> [!WARNING]
> This compiler is still in development and contains many bugs and missing features. Currently, this compiler is extremely unstable and shouldn't be used, so don't expect much.

## Requirements

- gcc
- make

## Installation

Clone the repository and run the makefile:

```console
$ git clone https://github.com/kinderjosh/reduced-cobol.git
$ cd reduced-cobol
```

### Unix-like

Simply run the makefile as root with the install flag:

```console
$ sudo make install
```

### Windows

1. Build the executable:

```console
$ make
```

2. Add the executable path to your environment variables.

### Uninstalling

Only available for Unix-likes:

```console
$ sudo make uninstall
```

## Usage

```
./cobc <command> [options] <files...>
```

| Command | Description |
| --- | --- |
| build | Produce an executable. |
| object | Produce an object file. |
| run | Build and run the executable. |
| source | Produce a C file. |

| Option | Description |
| --- | --- |
| -g | Build with debugging information. |
| -include ```<header>``` | Include a C header. |
| -l ```<library>``` | Link with a C library. |
| -no-main | Don't add a main function. |
| -o ```<output file>``` | Specify the output filename. |

## License

Reduced COBOL is distributed under the [MIT](./LICENSE) license.
