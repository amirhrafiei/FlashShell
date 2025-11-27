# ⚡ FlashShell

A lightweight **Unix-style shell** built in **C++** that serves as a functional, minimal alternative to Bash for learning and experimentation. FlashShell supports:

* Pipelines
* I/O Redirection
* Environment Variables
* Persistent Command History

FlashShell is built with a custom parser, several built-in commands, and the ability to launch external programs via `fork()` and `execvp()`.

---

## Features

### Built-in Commands

| Command | Description |
| :--- | :--- |
| `cd` | Change directories. |
| `pwd` | Print working directory. |
| `exit` | Quit FlashShell. |
| `history` | Persistent command history using GNU Readline. |

### Execution Features

* **Launch external programs** via `fork()` + `execvp()`.
* **Environment variable expansion:**
    ```bash
    flashshell> echo $HOME
    ```
* **Pipelines:** Supports chaining commands with pipes (`|`):
    ```bash
    flashshell> ls | grep cpp | wc -l
    ```
* **I/O Redirection:**
    * Input: `command < input.txt`
    * Output overwrite: `command > out.txt`
    * Output append: `command >> log.txt`

### Fully Usable Shell

At this stage FlashShell lets you:

* Run commands and see their output.
* Navigate the filesystem.
* Use pipelines and redirection.
* Maintain command history across sessions.
* Use environment variables.

---
#  Installation & Build Instructions

## Requirements

* **macOS or Linux** (POSIX-compliant systems)
* **C++17** compiler
* **GNU Readline** library (required for command history; macOS users must install manually)

---

## Installing Dependencies on macOS (via Homebrew)

FlashShell uses the **GNU `readline`** library for command history and line editing. Since macOS does not ship with the GNU version by default, you must install it.


```bash
/bin/bash -c "$(curl -fsSL [https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh](https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh))"
Add Homebrew to your PATH:

echo 'eval "$(/opt/homebrew/bin/brew shellenv)"' >> ~/.zprofile
eval "$(/opt/homebrew/bin/brew shellenv)"

Install GNU Readline

brew install readline

Building FlashShell

g++ -o flashshell main.cpp \
    -std=c++17 \
    -I/opt/homebrew/opt/readline/include \
    -L/opt/homebrew/opt/readline/lib \
    -lreadline -lncurses

Examples:

flashshell> pwd
flashshell> cd /usr/local
flashshell> echo $HOME
flashshell> ls | grep txt
flashshell> cat notes.txt > backup.txt
flashshell> cat error.log >> all.log
flashshell> sort < input.txt
flashshell> history
flashshell> exit
