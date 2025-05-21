# Shell Implementation

This project implements a custom shell with various features including command execution, process management, pipes, and command history.

## Features

- Basic command execution
- Process management (halt, wakeup, ice)
- Input/output redirection
- Pipeline support
- Command history
- Background process execution

## Building the Project

To build the project, simply run:

```bash
make
```

This will create two executables:
- `myshell`: The main shell implementation
- `mypipeline`: A standalone pipeline demonstration program

## Usage

### Running the Shell

```bash
./myshell
```

### Available Commands

- Basic commands: Any standard Linux command (ls, cat, etc.)
- Process management:
  - `halt <pid>`: Suspend a process
  - `wakeup <pid>`: Resume a suspended process
  - `ice <pid>`: Terminate a process
  - `procs`: List all processes
- History commands:
  - `hist`: Show command history
  - `!!`: Execute last command
  - `!n`: Execute command number n from history

### Examples

1. Basic command execution:
```bash
$ ls -l
```

2. Background process:
```bash
$ sleep 10 &
```

3. Pipeline:
```bash
$ ls | grep .c
```

4. Input/output redirection:
```bash
$ cat < input.txt > output.txt
```

## Project Structure

- `myshell.c`: Main shell implementation
- `mypipeline.c`: Pipeline demonstration program
- `LineParser.c/h`: Command line parsing utilities
- `looper.c`: Test program for process management
- `makefile`: Build configuration

## Requirements

- Linux/Unix environment
- GCC compiler
- Make build system

## License

This project is part of a lab assignment and is for educational purposes only. 