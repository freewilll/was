# WAS Assembler for x86_64 Linux

This project is an assembler for `x86-64` linux intended for use with [wcc](https://github.com/freewilll/wcc).

# Features
- Assembly based on [x86reference.xml](https://github.com/Barebit/x86reference/blob/master/x86reference.xml)
- A single `.text` section
- Multiple data sections
- Limited expressions such as used in `.size` and the debug symbols
- Branch shortening
- Debug symbols

# Building

A python script parsed `x86reference.xml` into `opcodes-generated.c`. A python virtualenv must be installed with some dependencies. To set this up:
```
cd scripts
python3 -m virtualenv venv
source venv/bin/activate
pip install -r requirements.txt
```

Build
```
make was
```

Run tests
```
make test
```
