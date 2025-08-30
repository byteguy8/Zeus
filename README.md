# Zeus Programming Language
The toy programming language Zeus (I now, ironic).

## Build
### Linux

```
cd zeus
mkdir build
make BUILD=RELEASE PLATFORM=LINUX
```

### Windows

```
cd zeus
mkdir build
make BUILD=RELEASE PLATFORM=WINDOWS
```

## Run
### Linux

```
./build/zeus /path/to/source/file
```

### Windows (PowerShell)

```
./build/zeus.exe /path/to/source/file
```

## Examples
### Values

```
empty;                   // equivalent of 'nil' in others languages
false;                   // booleans
9223372036854775807;     // 64 bits integers
3.142857143;             // 64 bits doubles
"Hi!"                    // immutable strings
array(0,1,2,4,5,6,7,8,9) // arrays
list(0,1,2,4,5,6,7,8,9)  // dynamic arrays
dict(                    // dictionaries
    "name" to "Zeus",
    "age" to 37
);
```

### Variables
    mut foo = false; // can change its value
    imut bar = 3.14; // cannot change its value

### Strings

Strings in Zeus are immutable

#### Indexing

```
imut foo = "Hello world!";

println(foo[0]);          // printing first character
println(foo[foo.len()]);  // printing last character
```

#### Concatenation

```
imut name = "Michael";
imut lastname = "Byte";
imut full_name = name .. " " .. lastname; // result: 'Michael Byte'
```

#### Multiplication

```
imut foo = "ha" ** 8; // result: hahahahahahahaha
```

### Data Structures
#### Arrays

You can create a empty array of x length:

```
imut values = array[1024];
println(values.len());
```

or just hardcode some values:

```
imut foo = array(empty, false, true, 2, 3.14);

println(foo[0]);         // getting the value at index 0
foo[0] = "Some text..."; // replacing value at index 0
println(foo[0]);         // printing new value at index 0
```
#### Lists

```
imut bar = list(empty, false, true, 2, 3.14);

println(foo[0]);           // getting value at index 0
foo[0] = "Some text...";   // setting value at index 0
```

#### Dicts

```
imut foo = dict(
    1 to "I will not waste chalk",
    2 to "I will not skateboard in the halls",
    3 to "I will not instigate revolution"
);

println(foo[2])                 // getting the value at key '2'
foo[4] = "I did not see Elvis"; // replacing the value at key '4'
```

### try/catch
You can use the throw statement with a value:

```
proc foo(){
    throw "error from foo"; // throw with value
}
```

or without:

```
proc bar(){
    throw; // throw without any value
}
```

If throw statement is used with a value, that value will be in the declared variable after the 'catch' keyword:

```
try{
    foo();
}catch err{
    println(err); // it prints 'error from foo'
}
```

otherwise, the value of the declared value will be 'empty'

```
try{
    bar();
}catch err{
    println(err); // it prints 'empty'
}
```

But you can also use catch without a variable, even if throw is used with some value

```
try{
    bar();
}catch{
    // some code...
}
```

### Conditional

```
mut sum0 = 0;
mut sum1 = 0;
mut sum2 = 0;

for(i = 1 upto 101){
    if(i <= 25){
        sum0 += i;
    }elif(i <= 50){
        sum1 += i;
    }else{
        sum2 += i;
    }
}

println("sum #1 (from 1 to 25): " .. to_str(sum0));
println("sum #2 (from 26 to 50): " .. to_str(sum1));
println("sum #3 (from 51 to 100): " .. to_str(sum2));
println("Gauss: " .. to_str(sum0 + sum1 + sum2));
```

### Loops
#### While

```
// PRINTING THE ALPHABET

imut alphabet = "abcdefghijklmnopqrstuvwxyz";
mut i = 0;

while(i < alphabet.len()){
    imut letter = alphabet[i];
    println(letter);

    i += 1;
}

// PRINTING EVEN NUMBERS IN INTERVAL [1, 100]

mut o = 1;

while(o <= 100){
    if(o mod 2 != 0){
        o += 1;
        continue;
    }

    println(o);

    io += 1;
}
```

#### For

```
// PRINTING THE ALPHABET

imut alphabet = "abcdefghijklmnopqrstuvwxyz";

// in normal order
for(i = 0 upto alphabet.len()){
    imut letter = alphabet[i];
    println(letter);
}

// in reverse order
for(i = 0 downto alphabet.len()){
    imut letter = alphabet[i];
    println(letter);
}

// PRINTING EVEN NUMBERS IN INTERVAL [1, 100]

for(num = 1 upto 101){
    if(num mod 2 == 0){
        println(num);
    }
}

// PRINTING THE FIRST 10 EVEN NUMBERS IN INTERVAL [1, 100]

mut even_count = 0;

for(num = 1 upto 101){
    if(num mod 2 == 0){
        even_count += 1;
        println(num);
    }

    if(even_count == 10){
        stop;
    }
}
```