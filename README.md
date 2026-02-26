# 🐚 xshll

![C](https://img.shields.io/badge/language-C-blue)
![POSIX](https://img.shields.io/badge/OS-POSIX-orange)
![License](https://img.shields.io/badge/license-educational-lightgrey)
![Build](https://img.shields.io/badge/build-gcc-brightgreen)
![Version](https://img.shields.io/badge/version-1.0-yellowgreen)

A **minimal Unix shell** written in C. Execute commands, chain them with pipes, and explore the fundamentals of process management on POSIX systems.

> 💡 Perfect for learning low-level Unix programming and how shells work under the hood.

---

## ⚡ Features

* 💻 Execute commands with arguments
* 🔀 Pipe commands (`|`)
* 📂 Built-ins: `cd`, `exit`
* 🧵 Single-threaded, minimal design
* ⚠️ No job control, background tasks, or advanced shell features

---

## 🛠 Requirements

* POSIX-compliant system (Linux, macOS, BSD)
* GCC or any C compiler

---

## 🚀 Build & Run

```bash
# Compile
gcc -o xshll src/xshll.c

# Run
./xshll
```

---

## 💡 Usage Examples

```bash
# Simple command
ls -l

# Command with arguments
grep "main" xshll.c

# Piping
ls -l | grep .c

# Change directory
cd src

# Exit shell
exit
```

---

## 🧠 How It Works

* Reads user input from terminal
* Parses commands and arguments
* Handles built-in commands (`cd`, `exit`)
* Forks child processes with `fork()`
* Executes commands via `execvp()`
* Connects processes with `pipe()`
* Waits for child processes using `waitpid()`

---

## ⚠️ Limitations

* No background jobs (`&`)
* No input/output redirection (`>`, `<`)
* Minimal error handling
* Educational / experimental use only

---

## 🌟 Possible Improvements

* 🔹 Add job control & background processes
* 🔹 Input/output redirection support
* 🔹 Command history & readline support
* 🔹 Advanced built-in commands
* 🔹 Signal handling enhancements

---

## 🤝 Contributing

Contributions are welcome! Feel free to fork, submit issues, or make pull requests to improve xshll.

* Fork it 🔁
* Clone it 📂
* Make your improvements ✨
* Submit a Pull Request 💌

---

## 📚 Educational Value

xshll is perfect for developers who want to understand:

* Unix process management
* Pipes and inter-process communication
* Minimal shell architecture
* How command execution works under the hood
