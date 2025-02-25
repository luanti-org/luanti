# Compiling on Windows using MSVC

## Introduction

-   All instructions use PowerShell unless otherwise noted
-   All commands are executed in `your/path/to/luanti` unless otherwise noted
-   We assume familiarity with Git in order to clone the repo
-   We assume you're compiling the portable build for 64-bit machines

Quick glossary:

-   MSVC: Microsoft Visual C++ is a C++ compiler for Windows
-   vcpkg: Cross-platform package manager for C++ libraries
-   cmake: Set of tools for configuring and building C++ projects

## Install machine dependencies

1. MSVC comes with Visual Studio 2022 when installed with C++ development, but the CI uses [VS 2019](https://visualstudio.microsoft.com/vs/older-downloads/)
2. vcpkg can be installed and added to the command line following guide <!-- todo guide -->
3. cmake CLI comes bundled with Visual Studio 2022 but needs to be added to path <!-- todo guide -->
