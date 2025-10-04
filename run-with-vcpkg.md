# Clone the Repository

```sh
git clone --recurse-submodules https://github.com/moadabdou/neutron.git
```

# Set Up vcpkg

## Windows

```sh
./external/vcpkg/bootstrap-vcpkg.bat
```

## Linux

```sh
./external/vcpkg/bootstrap-vcpkg.sh
```

# Configure the Project

## Windows (MSVC)

```sh
cmake --preset windows-msvc
```

## Linux (GCC)

```sh
cmake --preset linux-gcc
```

> **Note:** See `CMakePresets.json` for additional preset configurations.

# Build the Project

```sh
cmake --build --preset default
```