<img width="1664" height="928" alt="image" src="https://github.com/user-attachments/assets/b00f1786-3846-4266-9197-d3ad79fe8d73" />

# Official F Language Learning Guide

F is a modern programming language that supports static typing, object-oriented programming through structs and methods, reference-counting-based memory management, and advanced collections such as lists and maps.
This document is structured to help you systematically learn everything from the basic syntax of the F language to its advanced features.

---

## Table of Contents

1. Basic Syntax and Variable Declarations
2. Data Types
3. Operators and Expressions
4. Control Flow (Conditionals and Loops)
5. Functions and Recursion
6. Structs and Object-Oriented Programming
7. Collections (Arrays, Lists, Maps)
8. Strings and Memory Management
9. Module System and Import
10. Exception Handling and Runtime Behavior
11. Comprehensive Practical Examples

---

## 1. Basic Syntax and Variable Declarations

Every statement in the F language must end with a semicolon (`;`). Comments support both single-line comments using `//` and multi-line comments using `/* */`.

### 1.1 Writing Comments

Comments are text ignored by the compiler and are used to explain code.

    // This is a single-line comment.

    /*
    This is a multi-line comment.
    It is used for code explanations or documentation.
    */

### 1.2 Variable Declaration (let)

In the F language, variables are declared using the `let` keyword. Variables are immutable by default.

    let x = 10;
    let name = "F Language";

### 1.3 Mutable Variable Declaration (let mut)

Variables whose values need to be changed are declared using `let mut`.

    let mut counter = 0;
    counter = counter + 1; // The value can be changed

### 1.4 Type Annotations

F supports type inference, but types can also be specified explicitly. A type is specified after the variable name using a colon (`:`).

    let age: i64 = 25;
    let mut pi: f64 = 3.14159;
    let is_active: bool = true;

If no type is specified, the compiler automatically infers the type based on the assigned value.

---

## 2. Data Types

F provides a variety of built-in data types.

### 2.1 Integer Types

F supports both signed and unsigned integers in 32-bit and 64-bit sizes.

- `i32`: 32-bit signed integer
- `i64`: 64-bit signed integer (default integer type)
- `u32`: 32-bit unsigned integer
- `u64`: 64-bit unsigned integer

    let a: i32 = -100;
    let b: i64 = 9223372036854775807;
    let c: u32 = 4000000000;
    let d: u64 = 0;

### 2.2 Floating-Point Types

F supports floating-point types following the IEEE 754 standard for real-number calculations.

- `f32`: 32-bit single-precision floating-point
- `f64`: 64-bit double-precision floating-point (default floating-point type)

    let gravity: f64 = 9.81;
    let temp: f32 = 36.5;

### 2.3 Boolean Type

The logical type used to represent true and false.

- `bool`: Can contain only `true` or `false`.

    let is_valid: bool = true;
    let is_empty: bool = false;

### 2.4 String Type

A string type supporting UTF-8 encoding.

- `str`: Allocated on the heap and managed using reference counting.

    let greeting: str = "Hello, World!";

---

## 3. Operators and Expressions

F uses operator precedence similar to C and Rust.

### 3.1 Arithmetic Operators

Basic arithmetic operations are supported for both integers and floating-point values.

- `+`: Addition
- `-`: Subtraction
- `*`: Multiplication
- `/`: Division
- `%`: Remainder

    let sum = 10 + 5;
    let diff = 10 - 5;
    let prod = 10 * 5;
    let quot = 10 / 3;
    let rem = 10 % 3;

Floating-point calculations use the same operators.

    let f_sum = 10.5 + 2.5;
    let f_div = 10.0 / 3.0;

### 3.2 Comparison Operators

Comparison operators compare two values and return a `bool`.

- `==`: Equal
- `!=`: Not equal
- `<`: Less than
- `>`: Greater than
- `<=`: Less than or equal to
- `>=`: Greater than or equal to

    let is_equal = (10 == 10); // true
    let is_greater = (5 > 10); // false

### 3.3 Logical Operators

Used to combine or negate Boolean values.

- `&&`: Logical AND (short-circuit evaluation supported)
- `||`: Logical OR (short-circuit evaluation supported)

    let a = true;
    let b = false;

    let result_and = a && b; // false
    let result_or = a || b;  // true

### 3.4 Unary Operators

- `-`: Negative sign (integers and floating-point values)

    let neg = -10;
    let f_neg = -3.14;

---

## 4. Control Flow (Conditionals and Loops)

These statements control the execution flow of a program.

### 4.1 if-else Statement

Branches execution according to a condition. The condition must be of type `bool` or an integer/floating-point type.

    let x = 10;

    if x > 5 {
        print("x is greater than 5");
    } else if x == 5 {
        print("x is equal to 5");
    } else {
        print("x is less than 5");
    }

### 4.2 while Statement

Repeatedly executes the code inside the block while the condition is true.

    let mut count = 0;

    while count < 5 {
        print(count);
        count = count + 1;
    }

An infinite loop can be written using `while true`, and it can be exited using `return` from inside the loop.

    let mut i = 0;
    while true {
        if i >= 10 {
            return;
        }
        i = i + 1;
    }

---

## 5. Functions and Recursion

Functions are defined using the `fn` keyword.

### 5.1 Basic Function Definition

A function has a name, parameter list, and return type. If there is no return type, `void` may be specified or omitted.

    fn add(a: i64, b: i64) -> i64 {
        return a + b;
    }

    fn greet() {
        print("Hello from function!");
    }

### 5.2 Function Calls

Defined functions are called using their name and arguments. F uses a stack calling convention, so there is no limit on the number of arguments.

    let result = add(5, 10);
    print(result);

    greet();

### 5.3 Recursive Functions

A function can call itself from within its own body. Since recursive calls consume stack space, an appropriate termination condition is required.

    fn factorial(n: i64) -> i64 {
        if n <= 1 {
            return 1;
        }
        return n * factorial(n - 1);
    }

    let fact_5 = factorial(5);
    print(fact_5); // Outputs 120

---

## 6. Structs and Object-Oriented Programming

F supports object-oriented programming through `struct` and `impl`.

### 6.1 Struct Definition

The `struct` keyword is used to create user-defined data types.

    struct Point {
        x: f64,
        y: f64
    }

    struct User {
        id: i64,
        name: str,
        is_active: bool
    }

### 6.2 Creating Struct Instances

The `new` keyword is used to create a struct instance. All fields must be initialized.

    let p1 = new Point {
        x: 10.0,
        y: 20.0
    };

    let user1 = new User {
        id: 1,
        name: "Alice",
        is_active: true
    };

### 6.3 Field Access

The dot (`.`) operator is used to access fields of a struct.

    print(p1.x);
    print(user1.name);

    // Modify fields of a mutable instance
    let mut p2 = new Point { x: 0.0, y: 0.0 };
    p2.x = 5.0;
    p2.y = 15.0;

### 6.4 Method Definition (impl)

Methods can be added to a struct using an `impl` block.
An instance method receives `self` or `mut self` as its first parameter.

    impl Point {
        // Instance method
        fn distance_from_origin(self) -> f64 {
            return (self.x * self.x + self.y * self.y);
        }

        // Mutable instance method
        fn translate(mut self, dx: f64, dy: f64) {
            self.x = self.x + dx;
            self.y = self.y + dy;
        }
    }

### 6.5 Method Calls

Instance methods are called using the object name followed by a dot and the method name.

    let p = new Point { x: 3.0, y: 4.0 };
    let dist = p.distance_from_origin();
    print(dist);

    p.translate(1.0, 2.0);
    print(p.x); // Outputs 4.0

### 6.6 Static Methods

The `static` keyword can be used to define static methods that can be called using the class name without an instance.

    impl Point {
        static fn new_point(x: f64, y: f64) -> Point {
            return new Point { x: x, y: y };
        }
    }

    // Call
    let p3 = Point.new_point(5.0, 5.0);

---

## 7. Collections (Arrays, Lists, Maps)

F provides collections for managing data in groups.

### 7.1 Fixed-Size Array (Array)

A fixed-size array whose size is determined at compile time. The syntax is `[T; N]`.

    let arr = [1, 2, 3, 4, 5];
    print(arr[0]); // Outputs 1

    let mut arr2: [i64; 3] = [10, 20, 30];
    arr2[1] = 99;
    print(arr2[1]); // Outputs 99

Array indices start at 0, and accessing an element outside the valid range causes a runtime error.

### 7.2 Dynamic List (List)

A list whose size can change dynamically. It is declared using `List<T>`.

    let mut list = List<i64>.new();

    list.push(10);
    list.push(20);
    list.push(30);

    print(list.len()); // Outputs 3
    print(list.get(1)); // 20

    list.set(1, 99);
    print(list.get(1)); // 99

### 7.3 Map (Map)

A hash map that stores key-value pairs. It is declared using `Map<K, V>`.

    let mut map = Map<i64, str>.new();

    map.set(1, "One");
    map.set(2, "Two");
    map.set(3, "Three");

    let val = map.get(2);
    print(val); // "Two"

If the specified key does not exist, `get` returns a value corresponding to 0 or null.

---

## 8. Strings and Memory Management

F performs automatic reference-counting-based memory management for strings (`str`) and struct instances.

### 8.1 How Reference Counting Works

When a string or struct is assigned to a variable, its reference count increases by 1 (`retain`). When the variable's scope ends or another value is assigned, the reference count decreases by 1 (`release`). When the reference count reaches 0, the memory is automatically freed.

    fn create_string() -> str {
        let s = "Hello";
        return s; // Reference-counting logic is automatically inserted on return
    }

    let my_str = create_string();
    print(my_str);
    // Memory is freed when the scope of my_str ends

### 8.2 Explicit Memory Deallocation (delete)

Reference counting is normally handled automatically, but the `delete` keyword can be used when a circular reference occurs or when memory needs to be freed immediately.

    let mut s = "Temporary String";
    // Free the memory immediately after using s
    delete s;

`delete` can only be used with `str` and `struct` types.

---

## 9. Module System and Import

F manages files as modules. The `import` keyword is used to import code from another file.

### 9.1 Importing Modules

The `import` statement can only be used at the top level of a file and must end with a semicolon.

    import math;
    import utils;

When the compiler encounters `import math;`, it searches the current directory for the `math.fs` file and includes it in the compilation process. Files that have already been visited are not compiled again.

### 9.2 Module Example

math.fs:

    fn square(x: i64) -> i64 {
        return x * x;
    }

    fn cube(x: i64) -> i64 {
        return x * x * x;
    }

main.fs:

    import math;

    fn main() {
        let val = math.square(5);
        print(val); // Outputs 25
    }

---

## 10. Exception Handling and Runtime Behavior

F detects and handles certain errors at compile time and runtime.

### 10.1 Division by Zero

- **Compile time:** Division by zero between literal values produces a compilation error.

      let x = 10 / 0; // Compilation error: F1004: division by zero

- **Runtime:** When division by zero occurs using variables, the program does not crash and instead returns `0`.

      let a = 10;
      let b = 0;
      let c = a / b; // c becomes 0

### 10.2 Array and List Index Out of Bounds

Accessing an array or list outside its valid index range causes a runtime trap.

    let arr = [1, 2, 3];
    print(arr[5]); // Runtime error: runtime error: index out of bounds

If the index is negative or greater than or equal to the length, the program prints an error message and terminates immediately.

---

## 11. Comprehensive Practical Examples

These are practical examples combining various features of the F language.

### 11.1 Simple Calculator

A simple four-operation calculator using structs and methods.

    struct Calculator {
        result: f64
    }

    impl Calculator {
        static fn new() -> Calculator {
            return new Calculator { result: 0.0 };
        }

        fn add(mut self, value: f64) {
            self.result = self.result + value;
        }

        fn subtract(mut self, value: f64) {
            self.result = self.result - value;
        }

        fn multiply(mut self, value: f64) {
            self.result = self.result * value;
        }

        fn divide(mut self, value: f64) {
            if value != 0.0 {
                self.result = self.result / value;
            }
        }

        fn get_result(self) -> f64 {
            return self.result;
        }
    }

    fn main() {
        let mut calc = Calculator.new();

        calc.add(10.0);
        calc.multiply(5.0);
        calc.subtract(15.0);
        calc.divide(2.0);

        let final_result = calc.get_result();
        print(final_result); // Outputs 17.5
    }

### 11.2 Data Management Using a List

An example of storing and processing data using a dynamic list.

    fn main() {
        let mut numbers = List<i64>.new();

        // Add values from 1 to 5
        let mut i = 1;
        while i <= 5 {
            numbers.push(i * 10);
            i = i + 1;
        }

        // Print all elements
        let len = numbers.len();
        let mut idx = 0;
        while idx < len {
            print(numbers.get(idx));
            idx = idx + 1;
        }

        // Modify a specific element
        numbers.set(2, 999);
        print(numbers.get(2)); // Outputs 999
    }

### 11.3 Fibonacci Sequence Using Recursion

An example of calculating the Fibonacci sequence using recursive function calls.

    fn fibonacci(n: i64) -> i64 {
        if n <= 0 {
            return 0;
        }
        if n == 1 {
            return 1;
        }
        return fibonacci(n - 1) + fibonacci(n - 2);
    }

    fn main() {
        let mut i = 0;
        while i < 10 {
            let fib_val = fibonacci(i);
            print(fib_val);
            i = i + 1;
        }
    }

### 11.4 Calculating Word Frequency Using a Map (Concept)

An example of managing simple key-value data using a map.

    fn main() {
        let mut scores = Map<i64, i64>.new();

        scores.set(1, 100);
        scores.set(2, 85);
        scores.set(3, 92);

        // Update the score
        let current_score = scores.get(2);
        scores.set(2, current_score + 5);

        print(scores.get(2)); // Outputs 90
    }

---

## Appendix: F Language Compiler Architecture Summary

The F language compiler is written as a single C file and generates executables on its own without external libraries.

- **Lexer & Parser:** Tokenizes source code and converts it into an AST (Abstract Syntax Tree).
- **Optimizer:** Optimizes the AST through techniques such as Constant Folding.
- **Code Generation (Codegen):** Directly converts the AST into x86-64 machine code.
- **Runtime:** Memory allocation, reference counting, I/O functions, and other runtime functionality are included directly inside the binary.
- **Output Format:** On Windows, it directly assembles a PE32+ `.exe` file; on Linux, it directly assembles an ELF executable.

Through this guide, you should be able to sufficiently understand and use the core features of the F language. Additional feature extensions and syntax improvements will be implemented alongside updates to the compiler source code.
# F will be added later In the OS.. It could be otherwise, but I’m just too lazy  💀
