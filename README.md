# PYASM Compiler


    How to Use the Template:

    1. Replace [Project Name] with your compiler's name.
    1. Fill in placeholders like [language], [tool version], and other details specific to your project.
    1. You can expand on the sections, such as providing detailed usage instructions or more examples.

    Feel free to modify this based on your project's requirements!



## Table of Contents

1. [Introduction](#introduction)
1. [Team](#team)
1. [Features](#features)
1. [Installation](#installation)
1. [Usage](#usage)
1. [Examples](#examples)
1. [Testing](#testing)
1. [Acknowledgements](#acknowledgements)

---

## Introduction

The **PyAsm Compiler** is a C++ compiler designed to translates MiniPython (mpy) source code into AST representation and includes error checking at different compiling stage.

### Team

- Aymeric ROBERT
- Baptiste PACHOT
- Alexis CHAVY
- Johan Kasczyk

### Why use PyAsm?

- Don't use It.
- Fast compilation and easy to use.


## Features

- **Cross-platform:** Runs on [list of supported platforms]
- **Optimization:** Implements advanced optimizations like [list of optimizations: e.g., constant folding, dead code elimination]
- **Error Reporting:** Detailed syntax and semantic error messages
- **[Other features]**

## Installation

### Prerequisites

- **[Language/Tool Version(s)]:** Ensure you have [required language/tools] installed.

#### Example:

```bash
$ sudo apt-get install [required-dependencies]
```

### Installing the Compiler

Clone the repository and install any required dependencies:

```bash
$ git clone https://gibson.telecomnancy.univ-lorraine.fr/projets/2425/compil/[project-name].git
$ cd [project-name]
$ ./configure
$ make
$ sudo make install
```

## Usage

### Basic Command

To compile a [language] source file:

```bash
$ [project-name] [options] [source-file]
```

### Command-line Options

- `-o [file]`: Specify output file name
- `-O`: Enable optimizations
- `--debug`: Enable debug mode
- `--help`: Show help message

### Examples

#### Compiling a simple program:

```bash
$ [project-name] -o output.exe example.lang
```

#### Running in debug mode:

```bash
$ [project-name] --debug example.lang
```

## Examples

Here are some sample programs you can compile using [Project Name]:

1. **Hello World:**

```language
// Example code for Hello World in [language]
```

2. **Fibonacci Sequence:**

```language
// Example code for Fibonacci in [language]
```

For more examples, refer to the `examples/` directory.

## Testing

To run tests, make sure you have [testing framework] installed:

```bash
$ make test
```

You can also add specific test cases in the `tests/` folder and run them with:

```bash
$ ./run_tests.sh
```

### Reporting Issues

If you encounter any issues or have feature requests, please open an issue on the GitHub repository.

## Acknowledgements

- [Tool/Libraries] used in this project
- Special thanks to [contributors, advisors, etc.]

