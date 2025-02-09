# Zeus Programming Language
The toy programming language Zeus (I now, ironic).

## Build
### Linux
    mkdir build
    make
Plan to have windows built in the future

## Run
### Linux
    ./build/zeus /path/to/source/file

## Examples
### Variables
    mut foo = false; // can change its value
    imut bar = 3.14; // cannot change its value
### Strings
    // MULTIPLY
    imut header = ("*" * 5) + "ZEUS" + ("*" * 5); // result: *****ZEUS*****

    // CONCATENATION
    imut name = "Michael";
    imut lastname = "Byte";
    imut full_name = name + " " + lastname;

    // INTERPOLATION
    imut name = "Michael";
    imut lastname = "Bytes";
    imut full_name = "${name} ${lastname}";
### Data Structures
#### Arrays
    imut foo = array(empty, false, true, 2, 3.14);

    print foo[0];            // getting the value at 0
    foo[0] = "Some text..."; // setting a value at 0
#### Lists
    imut bar = list(empty, false, true, 2, 3.14);
    
    print foo.get(0);           // getting the value at 0
    foo.set(0, "Some text..."); // setting a value at 0
#### Dicts
    imut foo = dict(
        1 to "I will not waste chalk",
        2 to "I will not skateboard in the halls",
        3 to "I will not instigate revolution"
    );

    print foo.get(2);
    foo.put(4, "I did not see Elvis");
#### Records
    imut bar = record{
        name: "Michael",
        lastname: "Bytes",
        age: 37
    };

    print bar.name;
    bar.age = bar.age + 1;