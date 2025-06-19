# xshll

Minimal Unix shell clone written in C.  

---

## What is it?

xshll is a barebones command line shell implemented in C.  
It supports basic command execution with arguments and piping.  

Designed to get your hands dirty with process creation and signals in Unix.

---

## Features

- Executes commands with arguments  
- Supports piping (`|`)  
- Handles simple built-ins like `cd` and `exit`  
- No job control or advanced shell features  

---

## Requirements

- POSIX-compliant system (Linux, macOS, BSD)  
- GCC or compatible C compiler  

---

## Build & Run

```bash
gcc -o xshll src/xshll.c
./xshll
