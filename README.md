# skald
Binary Ninja plugin to recover RTTI vtables information from C++ binaries

## How to build

Clone the repository and initialize the submodules.

```commandline
git clone https://github.com/patacca/skald.git
cd skald
git submodule update --init
```

This will checkout the default version of the binary ninja API. Note that skald is compatible
with Binary Ninja >= 3.6.4762-dev, Build ID 27d9b06d, that means that you must use the
API past the commit [3ac99aa8](https://github.com/Vector35/binaryninja-api/commit/3ac99aa88c7019c8313304ef74dd5bbb468a74bc).

If you need to compile against a different version of the API you will have to specifically
pick the right commit:

```commandline
cd binaryninjapi
git checkout <your-commit>
```

Next use cmake to build the project

```commandline
cmake -B build/ -DBN_INSTALL_DIR=<path/to/binary/ninja>
cmake --build build
```

You can build it using clang or gcc using the `CC` and `CXX` env variables. You can also
sepcify a generator using the `-G` option of cmake, Ninja is the recommended one.

```commandline
CC=clang CXX=clang++ cmake -B build/ -DBN_INSTALL_DIR=<path/to/binary/ninja> -G Ninja
cmake --build build

# or if you prefer
cd build
ninja
```

Anf finally to install it:

```commandline
cmake --install build
```

You might want to specify a local install directory using `--prefix`, for example you can
install it on the binary ninja plugin directory:

```commandline
cmake --install build --prefix <path/to/binary/ninja/plugin>
```

## How to use it

After loading the binary, let binary ninja finish the analysis. Then go to `Plugin` > `skald`,
that will create all the relevant structures for the RTTI and vtables information.
