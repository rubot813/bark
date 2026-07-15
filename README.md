
# The Bark Programming Language

<img width="144" height="66" alt="woof" src="https://github.com/user-attachments/assets/2a3e2d9b-2859-4067-8f2e-13a3b3d85783" />

Bark is a minimal, low-level, imperative programming language powered by a lightweight stack-based virtual machine. It blends the simplicity of structured imperative syntax with the raw power, execution model, and memory mechanics of Forth.

---

## Architecture & Self-Hosting Model (Crucial)

Bark strictly separates the **Compilation** and **Execution** phases. Both the compiler engine and the virtual machine runner are exposed directly to the language itself as external C functions (`ext compile`, `ext exec`, etc.). 

This architectural decision enables a seamless **Self-Hosting Environment**:
1. You compile the native Bark core (`bark.c` + `bark_ext.c`) only once.
2. From that point forward, you can write tools *in Bark* that call the native compilation engine, generate new program images (ROMs), save them to disk, load them back, and execute them inside the VM.
3. This allows the system to easily bootstrap, modify, and extend itself without dropping back into native C development.

---

## Repository Structure

The repository is designed to be flat, transparent, and strictly organized:

* `bark.c` — The core system containing the bootstrap loader, lexer, statement/expression parser, compiler backend, and the stack-based Virtual Machine execution loop.
* `bark_ext.c` — The extended runtime environment. Rather than utilizing a heavy dynamic FFI, Bark implements dedicated wrapper functions that pop arguments off the VM stack, safely pass them to standard C library routines (40+ functions like `printf`, `strtok`, `fopen`), and route custom internal APIs.
* `/system` — The standard software ecosystem, written entirely in the Bark language:
  * `bb/` — Bark Base. The core operating shell. It handles loading/saving compiled program images (ROMs), orchestrates compilation/execution flows, and provides a built-in command-line shell (utilizing native `system()` hooks).
  * `bed/` — Bark Editor. A minimalist, line-oriented text editor heavily inspired by the classic Unix `ed` tool (currently under active development).
  * `test` — A comprehensive end-to-end integration test suite verifying every single primitive, memory instruction, and control flow mechanic of the Bark language.

---

## Key Features

- **Single Word Data Type**: Everything in Bark is a signed machine word (`intptr_t`). Its size dynamically adapts to your target architecture (32-bit or 64-bit).
- **Infix to Postfix Compilation**: Write standard infix expressions with mathematical operators and parentheses. The compiler automatically translates them into optimal postfix bytecode for the evaluation stack.
- **Explicit Stack & Memory Control**: Low-level memory mutations (`write`/`bread`) mixed with traditional register-less operations (`push`/`store`/`drop`).
- **Hardware-like Interrupts**: The `int` primitive acts as a soft-interrupt, instantly yielding VM execution control straight back to the hosting environment.

---

## Language Grammar (EBNF Specification)

<details>
<summary>Click to expand formal language grammar</summary>

```ebnf
(* Base conventions: 
   [...] - optional element (0 or 1 time)
   {...} - repetition (0 or more times)
   "..." - terminal token *)

(* A program consists of global variables/arrays and procedure definitions *)
program             = { global_declaration | procedure }

(* Global declarations (including static arrays) *)
global_declaration  = identifier [ "[" number "]" ]

(* Procedure definition *)
procedure           = "proc" identifier "{" { statement } "}"

(* Statements *)
statement           = assignment_stmt
                    | array_assignment_stmt
                    | if_stmt
                    | while_stmt
                    | push_stmt
                    | drop_stmt
                    | store_stmt
                    | write_stmt
                    | bwrite_stmt
                    | call_stmt
                    | ext_call_stmt
                    | interrupt_stmt
                    | expression (* Expressions can act as standalone statements *)

(* Assignments *)
assignment_stmt       = identifier "=" expression
array_assignment_stmt = identifier "[" expression "]" "=" expression

(* Control Flow *)
if_stmt             = "if" "(" expression ")" "{" { statement } "}" [ "else" "{" { statement } "}" ]
while_stmt          = "while" "(" expression ")" "{" { statement } "}"

(* Stack Primitives (Used for passing/retrieving parameters) *)
push_stmt           = "push" expression
drop_stmt           = "drop"
store_stmt          = "store" identifier

(* Memory Mutation Primitives *)
write_stmt          = "write" "(" expression ")" "=" expression ;  (* Word-size memory write *)
bwrite_stmt         = "bwrite" "(" expression ")" "=" expression ; (* Byte-size memory write *)

(* System Primitives *)
interrupt_stmt      = "int" ; (* Yields VM control back to the host system *)

(* Control & FFI Routine Invocation *)
call_stmt           = "call" identifier
ext_call_stmt       = "ext" identifier argument_list

(* Expressions & Infix Operations (Transformed into postfix during compilation) *)
argument_list       = "(" [ expression { expression } ] ")"
expression          = term { binary_op term }

term                = identifier
                    | identifier "[" expression "]" (* Array indexing *)
                    | number
                    | string_literal
                    | unary_op term  (* High-priority unary operators *)
                    | "(" expression ")"

(* Binary Operators (Ordered by priority matrix from * down to ||) *)
binary_op           = "*"  | "/"  | "%"
                    | "+"  | "-"
                    | ">>" | "<<"
                    | "<"  | ">"  | "<=" | ">="
                    | "==" | "!="
                    | "&"
                    | "^"
                    | "|"
                    | "&&"
                    | "||"

(* Unary Operators *)
unary_op            = "~"       (* Bitwise NOT (Highest priority) *)
                    | "read"    (* Read machine word (intptr_t) from memory *)
                    | "bread"   (* Read single byte from memory *)
                    | "addrof"  (* Get the address of a variable or array *)
                    | "iaddrof" (* Indirect address of for string literal pointers *)

(* Lexical Tokens *)
identifier          = letter { letter | digit | "_" }
number              = decimal_number | hex_number
decimal_number      = digit { digit }
octal_number        = "0" octal_digit { octal_digit }
hex_number          = "0x" hex_digit { hex_digit }
string_literal      = '"' { any_character_except_quote } '"'

digit               = "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9" ;
non_zero_digit      = "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9"
octal_digit         = "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7"
hex_digit           = digit | "a" | "b" | "c" | "d" | "e" | "f" | "A" | "B" | "C" | "D" | "E" | "F"
letter              = "a"..."z" | "A"..."Z"
```

</details>

---

## Memory Primitives Quick Reference

| Built-in Keyword | Operation | Description |
| :--- | :--- | :--- |
| `read(addr)` | Memory Read | Reads a platform-dependent word (`intptr_t`) from `addr`. |
| `bread(addr)` | Byte Read | Reads a single raw byte from `addr`. |
| `write(addr) = val` | Memory Write | Writes a platform-dependent word `val` into `addr`. |
| `bwrite(addr) = val` | Byte Write | Writes a single raw byte `val` into `addr`. |
| `addrof(var)` | Pointer Retrieval | Returns the raw memory address of a variable or array. |
| `iaddrof(str)` | Indirect Pointer | Resolves and returns the address pointer of a string literal. |

---

## Getting Started

### Prerequisites

You need a standard C compiler (GCC, Clang) or an IDE like **Code::Blocks (v25.03+)**.

### Building the Host Bootstrap Loader

Compile the driver file via terminal:

```bash
gcc -std=c11 -O2 main.c bark.c bark_ext.c -o bark
```

### Running Bark Code

The Bark source loader accepts the entry script path and routes any trailing numerical parameters straight to the VM execution stack:

```bash
./bark system/bb.bin 42 100
```
