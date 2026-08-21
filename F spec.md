# F Language Specification

Document Version: 6
Target Implementation: F Compiler v6
Document Type: Specification

This document defines the syntax and behavior of the F programming language in 100 items.
Each item consists of an explanation and an example.
Examples are written based on code that is actually compilable.

---

## 1. Language Name

The name of this language is F.
It uses a single uppercase alphabet letter as its name.
The compiler executable is named f_compiler.

Language Name: F
Compiler Name: f_compiler

---

## 2. Language Purpose

F is a programming language for system programming and general-purpose applications.
It converts source code into native executable files.
It operates without an interpreter.

Input: F source code
Output: Native executable file

---

## 3. Source File Extension

F source files use the .fs extension.
All source code is written in files with this extension.

main.fs
math.fs
calc.fs
player.fs

---

## 4. Output File Extension

Compiled output files use the .ft extension.
This file is an executable native binary.

./f_compiler main.fs main.ft

---

## 5. Character Encoding

F source files use UTF-8 encoding.
String literals may contain UTF-8 characters.

fn main() -> i32 {
    print("korean text\n");
    return 0;
}

---

## 6. Case Sensitivity

F is case-sensitive for identifiers.
Names with different letter cases are treated as different identifiers even if their spelling is otherwise identical.

let value = 1;
let Value = 2;
let VALUE = 3;

The three variables above are all different variables.

---

## 7. Whitespace

Whitespace consists of spaces, tabs, and line breaks.
Whitespace is used to separate tokens.
Multiple consecutive whitespace characters are treated as a single separator.

let x = 10;
let     y     =     20;

The two lines above have the same meaning with respect to whitespace.

---

## 8. Single-Line Comments

A single-line comment begins with //.
Everything from // to the end of the line is excluded from compilation.

fn main() -> i32 {
    // This line is a comment
    print("hi\n"); // This part is also a comment
    return 0;
}

---

## 9. Block Comments

A block comment is written between /* and */.
It may span multiple lines.

/*
This part
is all a comment
*/
fn main() -> i32 {
    return 0;
}

---

## 10. Nested Comments

Block comments cannot contain other block comments.
Nested block comments are not supported.

/*
Outer comment
/* Inner comment is not supported */
*/

The code above is not processed successfully.

---

## 11. Reserved Keywords

The reserved keywords used by F are listed below.

fn
let
mut
if
else
while
return
print
struct
new
delete
impl
self
static
import
true
false

---

## 12. Restrictions on Reserved Keywords

Reserved keywords cannot be used as variable names, function names, or struct names.
Using a reserved keyword as an identifier causes a compilation error.

let fn = 10;
let if = 20;

The code above is invalid.

---

## 13. Identifier Starting Characters

An identifier must begin with an English letter or an underscore.
It cannot begin with a number.

let value = 1;
let _value = 2;

The code above is valid.

let 1value = 3;

The code above is invalid.

---

## 14. Identifier Characters

The middle and remaining characters of an identifier may contain English letters, numbers, and underscores.
Whitespace and special characters are not allowed.

let player_score = 100;
let value2 = 200;
let max_hp_3 = 300;

---

## 15. Forbidden Identifier Characters

Spaces cannot be used inside identifiers.
Hyphens cannot be used inside identifiers.
Periods cannot be used inside identifiers.

let player score = 10;
let player-score = 20;

Both lines above are invalid.

---

## 16. Statement Termination

Most statements end with a semicolon.
A missing semicolon causes a compilation error.

let x = 10;
print("hi\n");
return 0;

---

## 17. Blocks and Semicolons

Blocks are enclosed in curly braces.
A semicolon is not placed after a block.

if x > 0 {
    print("positive\n");
}

A semicolon is not required after the closing brace.

---

## 18. i32 Type

i32 is a 32-bit integer type.
It can store integers in approximately the range of 2.1 billion in magnitude.

let count: i32 = 100;
let negative: i32 = -50;

---

## 19. i64 Type

i64 is a 64-bit integer type.
It can store a larger range of integers than i32.

let big: i64 = 10000000000;

---

## 20. bool Type

bool is a type with only two values: true and false.
It is used in conditional expressions.

let flag: bool = true;
let done: bool = false;

---

## 21. str Type

str is the string type.
String literals are enclosed in double quotation marks.

let name: str = "F language";

---

## 22. struct Type

struct is a user-defined type.
It can group multiple fields under a single name.

struct Point {
    x: i32,
    y: i32
}

---

## 23. Floating-Point Types Not Supported

F currently does not support floating-point types.
f32 and f64 cannot be used.
All numeric operations are performed using integers.

let value: f32 = 3.14;

The code above is invalid.

---

## 24. Integer Type Compatibility

i32 and i64 are compatible with each other.
Conversions between them are allowed for comparison and assignment.

let a: i32 = 10;
let b: i64 = a;

str and integer types are not compatible.

---

## 25. Integer Literals

Integer literals are written in decimal notation.

let a = 0;
let b = 100;
let c = 12345;

---

## 26. Negative Literals

Negative values are represented by placing the unary minus operator before a number or expression.

let negative = -10;
let result = 5 + -3;

---

## 27. Boolean Literals

true and false are boolean literals.

let is_ready = true;
let is_done = false;

---

## 28. String Literals

String literals are enclosed in double quotation marks.
An empty string is written as "".

let empty = "";
let hello = "hello";
let with_space = "hello world";

---

## 29. \n Escape Sequence

Within a string, \n represents a newline character.

fn main() -> i32 {
    print("line one\nline two\n");
    return 0;
}

Output:

line one
line two

---

## 30. \t Escape Sequence

Within a string, \t represents a tab character.

fn main() -> i32 {
    print("col1\tcol2\tcol3\n");
    return 0;
}

Output:

col1    col2    col3

---

## 31. \" Escape Sequence

Within a string, \" represents a double quotation mark.

fn main() -> i32 {
    print("quote: \"F\"\n");
    return 0;
}

Output:

quote: "F"

---

## 32. \\ Escape Sequence

Within a string, \\ represents a backslash character.

fn main() -> i32 {
    print("backslash: \\\n");
    return 0;
}

Output:

backslash: \

---

## 33. let Declaration

Variables are declared using let.
A variable must be initialized at the time it is declared.

let x = 10;
let name = "F";

---

## 34. let Immutability Rule

A variable declared with let cannot have its value changed.
Attempting to assign a new value causes a compilation error.

let x = 10;
x = 20;

The code above is invalid.

---

## 35. let mut Declaration

Variables whose values need to be changed are declared using let mut.

let mut x = 10;
x = 20;
x = 30;

The code above executes normally.

---

## 36. Declaration and Initialization

Variables must be initialized when declared.
A variable cannot be declared without a value.

let x;

The code above is invalid.

let x = 0;

The code above is valid.

---

## 37. Explicit Type Annotation

A variable's type may be specified after a colon.

let x: i32 = 10;
let y: i64 = 20;
let name: str = "F";
let flag: bool = true;

---

## 38. Type Inference

If the type is omitted, the compiler infers the type from the expression.

let x = 10;
let name = "F";
let flag = true;

In the code above, x is inferred as an integer, name as str, and flag as bool.

---

## 39. Duplicate Variable Names

Variable names cannot be duplicated within the same scope.

let x = 10;
let x = 20;

The code above is invalid.

---

## 40. Variables Must Be Declared Before Use

A variable must be declared before it is used.
Using an undefined variable causes a compilation error.

fn main() -> i32 {
    print(x);
    let x = 10;
    return 0;
}

The code above is invalid.

---

## 41. Addition Operator

The addition operator is +.

let a = 10;
let b = 20;
let sum = a + b;

The value of sum is 30.

---

## 42. Subtraction Operator

The subtraction operator is -.

let a = 100;
let b = 30;
let diff = a - b;

The value of diff is 70.

---

## 43. Multiplication Operator

The multiplication operator is *.

let a = 7;
let b = 6;
let product = a * b;

The value of product is 42.

---

## 44. Division Operator

The division operator is /.
Integer division returns only the quotient.

let a = 100;
let b = 7;
let quotient = a / b;

The value of quotient is 14.

---

## 45. Remainder Operator

The remainder operator is %.

let a = 100;
let b = 7;
let remainder = a % b;

The value of remainder is 2.

---

## 46. Integer Division Quotient

Integer division discards the fractional part and returns only the quotient.

let a = 10;
let b = 3;
let result = a / b;

The value of result is 3, not 3.333.

---

## 47. Comparison Operators

The comparison operators are ==, !=, <, >, <=, and >=.
Comparison results are 0 or 1.

let a = 10;
let b = 20;

let eq = a == b;
let ne = a != b;
let lt = a < b;
let gt = a > b;
let le = a <= b;
let ge = a >= b;

eq is 0, ne is 1, lt is 1, gt is 0, le is 1, and ge is 0.

---

## 48. Logical Operators

The logical operators are && and ||.
&& returns 1 when both operands are true.
|| returns 1 when at least one operand is true.

let a = 1;
let b = 0;

let and_result = a && b;
let or_result = a || b;

and_result is 0 and or_result is 1.

---

## 49. Operator Precedence

Multiplication and division are evaluated before addition and subtraction.
The remainder operator has the same precedence as multiplication and division.

let result = 10 + 20 * 3;

The value of result is 70, not 90.

---

## 50. Parentheses

Expressions inside parentheses are evaluated first.

let result = (10 + 20) * 3;

The value of result is 90.

---

## 51. Unary Minus

The unary minus operator reverses the sign of a value.

let a = 10;
let b = -a;
let c = -b;

b is -10 and c is 10.

---

## 52. Function Definition

Functions are defined using fn.
A parameter list is written in parentheses after the function name.

fn add(a: i32, b: i32) -> i32 {
    return a + b;
}

---

## 53. Parameter Declarations

Parameter types must always be specified.
Omitting a parameter type causes a compilation error.

fn add(a, b) -> i32 {
    return a + b;
}

The code above is invalid.

fn add(a: i32, b: i32) -> i32 {
    return a + b;
}

The code above is valid.

---

## 54. Return Type Declaration

The return type is specified after the parameter list using ->.

fn get_value() -> i32 {
    return 42;
}

---

## 55. void Return Type

If the return type is omitted, the function has a void return type.
A void function does not return a value.

fn greet() {
    print("hello\n");
}

---

## 56. return Statement

The return statement returns a value.
When return is encountered, function execution terminates immediately.

fn check(x: i32) -> i32 {
    if x > 0 {
        return 1;
    }
    return 0;
}

---

## 57. Required main Function

A program must contain exactly one main function.
Execution begins from the main function.

fn main() -> i32 {
    print("program start\n");
    return 0;
}

If main is missing, a compilation error occurs.

---

## 58. Exit Code

The return value of main becomes the program's exit code.
0 indicates normal termination.
A non-zero value indicates abnormal termination.

fn main() -> i32 {
    return 0;
}

---

## 59. Function Calls

Functions are called using their name followed by parentheses.
Arguments are placed inside the parentheses.

fn add(a: i32, b: i32) -> i32 {
    return a + b;
}

fn main() -> i32 {
    let result = add(10, 20);
    print(result);
    return 0;
}

---

## 60. Duplicate Function Names

Function names cannot be duplicated.
Defining two functions with the same name causes a compilation error.

fn test() {
    print("one\n");
}

fn test() {
    print("two\n");
}

The code above is invalid.

---

## 61. if Statement

if executes a block when its condition is true.
A condition is considered true when its value is non-zero.

fn main() -> i32 {
    let x = 10;

    if x > 0 {
        print("positive\n");
    }

    return 0;
}

---

## 62. Condition Value Rules

The conditions of if and while must be integer or bool values.
Strings cannot be used as conditions.

if name {
    print("invalid\n");
}

If name has type str, the code above is invalid.

---

## 63. else Statement

An else block executes when the condition is false.

fn main() -> i32 {
    let x = 0;

    if x > 0 {
        print("positive\n");
    } else {
        print("not positive\n");
    }

    return 0;
}

---

## 64. else if Statement

else if can be used to chain multiple conditions.

fn main() -> i32 {
    let score = 85;

    if score >= 90 {
        print("A\n");
    } else if score >= 80 {
        print("B\n");
    } else if score >= 70 {
        print("C\n");
    } else {
        print("F\n");
    }

    return 0;
}

---

## 65. while Statement

while repeatedly executes a block while its condition is true.

fn main() -> i32 {
    let mut i = 0;

    while i < 5 {
        print(i);
        print("\n");
        i = i + 1;
    }

    return 0;
}

Output:

0
1
2
3
4

---

## 66. Nested if

An if statement may contain another if statement.

fn main() -> i32 {
    let x = 10;
    let y = 20;

    if x > 0 {
        if y > 0 {
            print("both positive\n");
        }
    }

    return 0;
}

---

## 67. Nested while

A while loop may contain another while loop.

fn main() -> i32 {
    let mut i = 0;

    while i < 3 {
        let mut j = 0;
        while j < 3 {
            print("*");
            j = j + 1;
        }
        print("\n");
        i = i + 1;
    }

    return 0;
}

Output:

***
***
***

---

## 68. print Function

print outputs a value to the screen.
It can output integers and strings.

fn main() -> i32 {
    print("hello\n");
    print(100);
    return 0;
}

---

## 69. Integer Output

Passing an integer to print outputs the number.

fn main() -> i32 {
    print(42);
    print("\n");
    print(-10);
    print("\n");
    return 0;
}

Output:

42
-10

---

## 70. String Output

Passing a string to print outputs the string as-is.

fn main() -> i32 {
    print("F language\n");
    return 0;
}

Output:

F language

---

## 71. print Does Not Add a Newline

print does not automatically add a newline after output.
If a newline is required, \n must be included in the string.

fn main() -> i32 {
    print("one");
    print("two");
    print("\n");
    return 0;
}

Output:

onetwo

---

## 72. struct Definition

A struct is defined using the struct keyword.
The struct name is followed by a field list enclosed in braces.

struct Point {
    x: i32,
    y: i32
}

---

## 73. struct Fields

A struct contains a list of fields.
Each field has a name and a type.

struct Player {
    hp: i32,
    mp: i32,
    name: str
}

---

## 74. struct Field Types

Struct fields may use i32, i64, and str types.

struct Item {
    id: i32,
    count: i64,
    name: str
}

---

## 75. new Construction

Struct values are created using new.
new creates the value in heap memory.

struct Point {
    x: i32,
    y: i32
}

fn main() -> i32 {
    let p = new Point {
        x: 10,
        y: 20
    };

    delete p;
    return 0;
}

---

## 76. Field Initialization Rules

All fields must be initialized when using new.
If even one field is missing, a compilation error occurs.

struct Point {
    x: i32,
    y: i32
}

fn main() -> i32 {
    let p = new Point {
        x: 10
    };

    delete p;
    return 0;
}

The code above is invalid because the y field is missing.

---

## 77. Field Access

A field is accessed by placing a period followed by the field name after the variable name.

struct Point {
    x: i32,
    y: i32
}

fn main() -> i32 {
    let p = new Point {
        x: 10,
        y: 20
    };

    print(p.x);
    print("\n");
    print(p.y);
    print("\n");

    delete p;
    return 0;
}

Output:

10
20

---

## 78. Field Modification

Fields can only be modified through a variable declared with let mut.

struct Point {
    x: i32,
    y: i32
}

fn main() -> i32 {
    let mut p = new Point {
        x: 10,
        y: 20
    };

    p.x = 100;

    print(p.x);
    print("\n");

    delete p;
    return 0;
}

Output:

100

Modifying a field through a variable declared with let causes a compilation error.

---

## 79. Heap Memory

Values created with new are stored in heap memory.
Heap memory remains allocated after the function that created it ends.
Heap memory must be released using delete.

struct Data {
    value: i32
}

fn main() -> i32 {
    let d = new Data {
        value: 100
    };

    delete d;
    return 0;
}

---

## 80. delete Statement

delete releases heap memory.
It can only be used with values created by new.

struct Data {
    value: i32
}

fn main() -> i32 {
    let d = new Data {
        value: 100
    };

    delete d;
    return 0;
}

---

## 81. Reference Counting

Strings are managed using reference counting.
When a string is assigned to a variable, its reference count increases.
When the string is released using delete, its reference count decreases.

fn main() -> i32 {
    let s = "hello";
    let t = s;

    delete s;
    delete t;

    return 0;
}

---

## 82. Duplicate delete Calls

The same value must not be passed to delete twice.
Double-free is undefined behavior.

struct Data {
    value: i32
}

fn main() -> i32 {
    let d = new Data {
        value: 100
    };

    delete d;
    delete d;

    return 0;
}

The second delete causes a problem.

---

## 83. impl Block

Methods are defined inside an impl block.
The name of the struct is written after impl.

struct Point {
    x: i32,
    y: i32
}

impl Point {
    fn sum(self) -> i32 {
        return self.x + self.y;
    }
}

---

## 84. Instance Method Definition

The first parameter of an instance method is self.
self refers to the object on which the method was called.

struct Counter {
    value: i32
}

impl Counter {
    fn get(self) -> i32 {
        return self.value;
    }
}

---

## 85. self Parameter

self can be used to access fields.
self.x refers to the x field of the current object.

struct Point {
    x: i32,
    y: i32
}

impl Point {
    fn sum(self) -> i32 {
        return self.x + self.y;
    }
}

---

## 86. mut self Parameter

mut self must be used when modifying the fields of self.

struct Counter {
    value: i32
}

impl Counter {
    fn increment(mut self) {
        self.value = self.value + 1;
    }
}

Fields cannot be modified without mut self.

---

## 87. Static Method Definition

Static methods are defined using static fn.
Static methods do not use self.

struct Math {
    value: i32
}

impl Math {
    static fn answer() -> i32 {
        return 42;
    }
}

---

## 88. Instance Method Calls

Instance methods are called using variable.method().

struct Point {
    x: i32,
    y: i32
}

impl Point {
    fn sum(self) -> i32 {
        return self.x + self.y;
    }
}

fn main() -> i32 {
    let p = new Point {
        x: 3,
        y: 4
    };

    print(p.sum());
    print("\n");

    delete p;
    return 0;
}

Output:

7

---

## 89. Static Method Calls

Static methods are called using Struct.method().

struct Math {
    value: i32
}

impl Math {
    static fn answer() -> i32 {
        return 42;
    }
}

fn main() -> i32 {
    print(Math.answer());
    print("\n");
    return 0;
}

Output:

42

---

## 90. import Statement

import loads another file.
The module name is written after import and the statement ends with a semicolon.

import math;

The code above loads the math.fs file.

---

## 91. Module File Resolution

import math; searches for math.fs in the same directory.
If the file does not exist, a compilation error occurs.

project/
├── main.fs
└── math.fs

If main.fs contains import math;, math.fs is loaded.

---

## 92. Duplicate Import Prevention

The same file is not loaded more than once.
Even if import is written multiple times, the file is included only once.

import math;
import math;
import math;

The code above loads math.fs only once.

---

## 93. Compilation Command

Compilation is performed using f_compiler.
The first argument is the input file, and the second argument is the output file.

./f_compiler main.fs main.ft

---

## 94. Granting Execute Permission

The output file must be given execute permission before it can be run.

chmod +x main.ft

---

## 95. Execution

After granting execute permission, the file can be executed.

./main.ft

The exit code can be checked using echo $?.

./main.ft
echo $?

---

## 96. Undefined Variable Error

Using an undefined variable causes a compilation error.

fn main() -> i32 {
    print(x);
    return 0;
}

The code above is invalid because x is not defined.

---

## 97. Type Mismatch Error

An assignment with incompatible types causes a compilation error.

fn main() -> i32 {
    let x: i32 = "hello";
    return 0;
}

The code above is invalid because a str value is assigned to an i32 variable.

---

## 98. Immutable Variable Assignment Error

Assigning a new value to a variable declared with let causes a compilation error.

fn main() -> i32 {
    let x = 10;
    x = 20;
    return 0;
}

The code above is invalid because x is immutable.

---

## 99. Division by Zero Error

Dividing by zero causes a runtime error.
It is not detected at compile time.

fn main() -> i32 {
    let a = 10;
    let b = 0;
    let result = a / b;
    return 0;
}

The code above terminates with a runtime error.

---

## 100. Missing Field Error

If even one field is missing when using new, a compilation error occurs.

struct Point {
    x: i32,
    y: i32,
    z: i32
}

fn main() -> i32 {
    let p = new Point {
        x: 1,
        y: 2
    };

    delete p;
    return 0;
}

The code above is invalid because the z field is missing.

---

## Complete Example Program 1: Calculator

fn print_num(n: i32) {
    print(n);
}

fn main() -> i32 {
    print("=== F Calculator ===\n");

    let a = 100;
    let b = 7;

    print("a = "); print_num(a); print("\n");
    print("b = "); print_num(b); print("\n\n");

    print("a + b = ");
    print_num(a + b);
    print("\n");

    print("a - b = ");
    print_num(a - b);
    print("\n");

    print("a * b = ");
    print_num(a * b);
    print("\n");

    print("a / b = ");
    print_num(a / b);
    print("\n");

    print("a % b = ");
    print_num(a % b);
    print("\n\n");

    print("(a + b) * 3 - 10 = ");
    print_num((a + b) * 3 - 10);
    print("\n");

    print("a > b = ");
    print_num(a > b);
    print("\n");

    print("a == 100 = ");
    print_num(a == 100);
    print("\n");

    let mut result = 0;
    if b != 0 {
        result = a / b;
    } else {
        print("ERROR: division by zero\n");
    }

    print("safe div result = ");
    print_num(result);
    print("\n");

    print("\n=== Done ===\n");
    return 0;
}

---

## Complete Example Program 2: struct and Methods

struct Point {
    x: i32,
    y: i32
}

impl Point {
    fn sum(self) -> i32 {
        return self.x + self.y;
    }

    fn add(mut self, value: i32) -> i32 {
        self.x = self.x + value;
        return self.x;
    }

    static fn answer() -> i32 {
        return 42;
    }
}

fn main() -> i32 {
    print("=== Point Example ===\n");

    let mut p = new Point {
        x: 3,
        y: 4
    };

    print("p.sum() = ");
    print(p.sum());
    print("\n");

    print("p.add(10) = ");
    print(p.add(10));
    print("\n");

    print("p.sum() after add = ");
    print(p.sum());
    print("\n");

    print("Point.answer() = ");
    print(Point.answer());
    print("\n");

    delete p;

    print("=== Done ===\n");
    return 0;
}

---

## Complete Example Program 3: import and Modules

math.fs:

fn square(x: i32) -> i32 {
    return x * x;
}

fn cube(x: i32) -> i32 {
    return x * x * x;
}

main.fs:

import math;

fn main() -> i32 {
    print("=== Import Example ===\n");

    let value = 5;

    print("square(5) = ");
    print(square(value));
    print("\n");

    print("cube(5) = ");
    print(cube(value));
    print("\n");

    print("=== Done ===\n");
    return 0;
}

---

## Compilation and Execution Procedure

# Step 1: Compile
./f_compiler main.fs main.ft

# Step 2: Grant execute permission
chmod +x main.ft

# Step 3: Execute
./main.ft

# Step 4: Check the exit code
echo $?

---

## Error Message Format

Compilation errors are printed in the following format.

F Compiler Error: <error message>

Linker errors are printed in the following format.

Link Error: undefined function '<function name>'

---

## Document End

This document is version 6 of the F Language Specification.
It consists of 100 items in total.
The examples are based on code that is compilable with F Compiler v6.
