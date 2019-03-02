# CFI-LB: Adaptive Call-site Sensitive Control Flow Integrity

CFI-LB: Adaptive Call-site Sensitive Control Flow Integrity will publish in EuroS&P' 2019. The source code is available here. The protoype is build with Clang/LLVM, Intel pin, and Triton (Symbolic Execution Engine), each of them have multiple dependencies. To build Clang/LLVM, it requires 20GB memory along, so make sure your machine can support the load. The run.sh may ask for sudo permission to install dependent library and enable/disable ASLR for process memory dump to use in concolic process.

## Project Structure
**CFILB reference monitor implementation:** cfilbLibs/

**Reference monitor instrumentation:** llvm-project/clang/lib/CodeGen/CGCall.cpp

**Clang libtool to prepare the source:** llvm-project/clang/tools/clangCodePrep/

**Intel pin dynamic CFG generator:** dCFG/

**Intel pin process memory dump:** cHelper/

**LLVM static CFG generator:** llvm-project/llvm/lib/Transforms/sCFG/

**LLVM instrument CFG:** llvm-project/llvm/lib/Transforms/instCFG

**Symbol table extraction:** utils/extract.py

**Adaptive filter algorithm:** utils/filter.py

**Concolic CFG generator:** cCFG/

## Overall Process
Step 1: A clang libtool will prepare the target code base.

Step 2: Copy the CFILB runtime library to the source directory.

Step 3: Build the source with clang (with reference monitor instrumentation) and generate the bitcode.

Step 4: Run a LLVM Pass analysis to calculate the static CFG and instrument the table back to bitcode.

Step 5: Build the binary from the step 4 bitcode. (This binary is protected with static CFG)

Step 6: Extract symbol table from the elf binary.

Step 7: Execute the binary with seed input using intel pin tool to generate dynamic CFG.

Step 8: Execute the binary with seed input using intel pin tool to dump memory for concolic process. (ASLR Disabled) [slow process]

Step 9: Run the concolic CFG generator with dump info from step 8. [can have crash issue, please report]

Step 10: Run a python script to apply the adaptive algorithm.

Step 11: Run another LLVM Pass to instrument the adaptive dynamic CFG table in the bitcode.

Step 12: Build the final binary from the step 9 bitcode. The binary will be named as: benchmarkname_cfg

## Limitation on Concolic CFG
Concolic CFG is build on Triton symbolic engine. Its outcome has randomness, so you may not see similar results for every run. Our tool implementation is based on Triton Qemu emulation support, so, we have to write a emulation abi for libc library (for C only). Due to incompleteness of the prototype, the concolic CFG is only effective for Cint benchmark. **Note: this is only an engineering limitation. We expect the open source community will adopt the concept and help us for further improvements.**

## Installation Guideline
1. Install required binary:
```text
sudo apt install clang cmake subversion g++ gcc bash git python-pip libcapstone-dev libboost-all-dev libz3-dev
pip install pyelftools
```

2. Git clone the project:
```text
git clone git@github.com:mustakcsecuet/CFI-LB.git
```
***Note: You can skip step 2, 3, 4, 6, and 7 if you have already configured Gold plugin for another compiler.***

3. Install required library for Gold plugin:
```text
sudo apt-get install linux-headers-$(uname -r) csh gawk automake libtool bison flex libncurses5-dev
# Check 'makeinfo -v'. If 'makeinfo' does not exist
sudo apt-get install apt-file texinfo texi2html
sudo apt-file update
sudo apt-file search makeinfo
```

4. Download binutils source code:
```text
cd ~
git clone --depth 1 git://sourceware.org/git/binutils-gdb.git binutils
```

5. Build binutils:
```text
mkdir build
cd build
../binutils/configure --enable-gold --enable-plugins --disable-werror
make
```

6. Build the compiler (use the binutils directory if you already have one):
```text
cd ../llvm-project
mkdir build
cmake -DLLVM_BINUTILS_INCDIR="path_to_binutils/include"  -DLLVM_ENABLE_PROJECTS=clang -G "Unix Makefiles" ../llvm
make -j8
sudo make install
```

7. Backup ar, nm, ld and ranlib:
```text
cd ~
mkdir backup
cd /usr/bin/
cp ar ~/backup/
cp nm ~/backup/
cp ld ~/backup/
cp ranlib ~/backup/
```

8. Replace ar, nm, ld and ranlib:
```text
cd /usr/bin/
sudo cp ~/build/binutils/ar ./
sudo rm nm
sudo cp ~/build/binutils/nm-new ./nm
sudo cp ~/build/binutils/ranlib ./
sudo cp ~/build/gold/ld-new ./ld
```

9. install LLVMgold.so to /usr/lib/bfd-plugins:
```text
cd /usr/lib
sudo mkdir bfd-plugins
cd bfd-plugins
sudo cp /path_to_llvm_project_build/lib/LLVMgold.so ./
sudo cp /path_to_llvm_project_build/lib/libLTO.* ./
```

10. Download intel-pin-3.5 source:
```text
cd /path_to_project_directory/
wget http://software.intel.com/sites/landingpage/pintool/downloads/pin-3.5-97503-gac534ca30-gcc-linux.tar.gz
tar -xvzf pin-3.5-97503-gac534ca30-gcc-linux.tar.gz
rm pin-3.5-97503-gac534ca30-gcc-linux.tar.gz
mv pin-3.5-97503-gac534ca30-gcc-linux intel-pin
```

11. Build the dynamic CFG generation pin:
```text
cd ../dCFG
make PIN_ROOT=../intel-pin/
make PIN_ROOT=../intel-pin/ obj-intel64/dCFG.so
```

## Spec Benchmark Build Guideline
1. Use spec2006-cfilb.cfg to build the spec cpu2006 benchmark.
2. Change the Makefile.spec in the build directory of the benchmark:
```text
# add cfilb.c in the source list, keep others same
SOURCES=cfilb.c ...
#if you have not installed the clang/llvm, then change
CC               = clang ...
# to
CC               = /path_to_llvm_project/build/bin/clang ..
# and
CXX              = clang++ ...
# to
CXX              = /path_to_llvm_project/build/bin/clang++
```
3. Change the run.sh (line 68)(follow the comments there).
4. Use the run.sh to start the system.