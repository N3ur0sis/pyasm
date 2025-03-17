# **PyAsm Compiler**

## **Table of Contents**
1. [Introduction](#introduction)  
2. [Team](#team)  
3. [Features](#features)  
4. [Installation](#installation)  
5. [Usage](#usage)  
6. [Examples](#examples)  
7. [Testing](#testing)  
8. [Acknowledgements](#acknowledgements)  

---

## **Introduction**
The **PyAsm Compiler** is a compiler implemented in **C++**, designed to **parse and analyze MiniPython (MPY) source code** by generating an Abstract Syntax Tree (AST) and performing error validation at various compilation stages. PyAsm enables static analysis of MiniPython scripts and facilitates structured representation of the input source.

## **Team**
- **Aymeric ROBERT**  
- **Baptiste PACHOT**  
- **Alexis CHAVY**  
- **Johan Kasczyk**  

## **Why Use PyAsm?**
- **Optimized Parsing and AST Generation** – Efficient processing of MiniPython syntax.  
- **Error Detection** – Provides structured error reporting for syntax and semantic issues.  
- **Designed for Educational and Research Purposes** – A tool for understanding compiler design in an academic setting.  
- **Cross-Platform Compatibility** – Can be compiled and executed on **Linux and macOS**.

---

## **Features**
- **Lexical and Syntax Analysis** – Tokenization and parsing of MiniPython scripts.  
- **AST Generation** – Converts source code into a structured AST representation.  
- **Error Handling and Reporting** – Detects and reports syntax errors with precision.  
- **Graphical AST Visualization** – Generates **DOT format output** for AST representation.  
- **Modular Architecture** – Easily extendable for additional features such as optimizations.  
- **Command-Line Interface** – Configurable options for analysis and debugging.

---

## **Installation**

### **Prerequisites**
Ensure that the following dependencies are installed:
- **C++20** (GCC, Clang, or MSVC)
- **CMake (>= 3.22)**
- **Graphviz (for AST visualization)**
- **Make (or an equivalent build system)**
- **Git**

#### **Linux/macOS Installation**
```bash
./scripts/install_deps.sh  # Installs required dependencies
```


### **Cloning the Repository**
```bash
git clone https://gibson.telecomnancy.univ-lorraine.fr/projets/2425/compil/pyasm.git
cd pyasm
```

### **Building the Compiler**
```bash
./scripts/build.sh  # Builds the project
```

---

## **Usage**

### **Basic Command Syntax**
```bash
./scripts/run.sh <source-file>
```

### **Command-Line Options**
| Option        | Description |
|--------------|-------------|
| `-o <file>`  | Specify the output file name |
| `--debug`    | Enable debug mode |
| `--dot <file>` | Export AST to DOT format for visualization |
| `--help`     | Display usage information |

### **Example Compilation**
```bash
./scripts/run.sh ./data/examples/ex1.mpy
```

### **Running in Debug Mode**
```bash
./scripts/run.sh --debug ./data/examples/ex1.mpy
```

---

## **Examples**
### **1. MiniPython Hello World**
```python
print("Hello, world!")
```
```bash
./scripts/run.sh ./data/examples/hello.mpy
```

### **2. Fibonacci Sequence in MiniPython**
```python
def fibonacci(n):
    if n <= 1:
        return n
    else:
        return fibonacci(n-1) + fibonacci(n-2)

print(fibonacci(10))
```
```bash
./scripts/run.sh ./data/examples/fibonacci.mpy
```

---

## **Testing**
To validate the functionality of **PyAsm**, run the test suite:
```bash
./scripts/full_setup.sh  # Installs dependencies, builds, and runs the project
```
or execute individual test scripts:
```bash
./scripts/run_tests.sh
```
### **Debugging and AST Visualization**
```bash
./scripts/run.sh --dot ast.dot ./data/examples/ex1.mpy
dot -Tpdf ast.dot -o ast.pdf
```
This generates an **AST visualization in PDF format**.

---

## **Acknowledgements**
This project utilizes:
- **CMake** – Build system  
- **Graphviz** – AST visualization  
- **GCC / Clang** – Compilation  
- **GitLab** – Version control  

Special thanks to **Telecom Nancy** for hosting the project infrastructure.

