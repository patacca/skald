# skald
Binary Ninja plugin to recover RTTI and vtables information from C++ stripped binaries.

> [!WARNING]
> The plugin is still under development and is not ready for production yet. Use it with cautious.

> [!IMPORTANT]
> This plugin works only for binaries that adhere to the Itanium C++ ABI, like gcc or clang on linux

## Features

The plugin is still under heavy development and most of the features are not yet implemented.

- [x] Recover RTTI
- [ ] Recover vtables (partially supported)
- [ ] Recover layout of objects and auto create struct
- [ ] Add support for 32bit arch
- [ ] Add support for ARM C++ ABI

## Dependencies

- C++23 compatible compiler
- libc++ standard library
- cmake >= 3.24
- Binary Ninja >= 3.6.4762-dev, Build ID 27d9b06d
- Binary Ninja C++ API >= [3ac99aa8](https://github.com/Vector35/binaryninja-api/commit/3ac99aa88c7019c8313304ef74dd5bbb468a74bc).

## How to build

Clone the repository and initialize the submodules.

```commandline
git clone https://github.com/patacca/skald.git
cd skald
git submodule update --init
```

> [!NOTE]
> This will checkout the default version of the binary ninja API. If you need to compile against a different version of Binary Ninja follow [Update Binary Ninja API](#update-binary-ninja-api)

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

And finally to install it:

```commandline
cmake --install build
```

You might want to specify a local install directory using `--prefix`, for example you can
install it on the binary ninja plugin directory:

```commandline
cmake --install build --prefix <path/to/binary/ninja/plugin>
```

### Cmake options

here is a list of all the cmake options available:

- `FORCE_COLORED_OUTPUT` to force the color usage during compilation

### Update Binary Ninja API

Binary Ninja C++ plugins need to be compiled against the specific API version that will be used.
The version of the API that your BN is using is written in the file `/path/to/binary-ninja/api_REVISION.txt`

```commandline
awk 'match($0, /tree\/([a-z0-9]*)$/, m) {print m[1]}' api_REVISION.txt
```

Once you have the correct commit you have to checkout `binaryninjaapi`:

```commandline
cd binaryninjaapi
git checkout <your-commit>
```

## How to use it

After loading the binary, let binary ninja finish the analysis. Then go to `Plugin` > `skald`,
that will create all the relevant structures for the RTTI and vtables information.
