# CFI-LB: Adaptive Call-site Sensitive Control Flow Integrity

CFI-LB: Adaptive Call-site Sensitive Control Flow Integrity will publish in EuroS&P' 2019. The source code is available here.

## Project Structure
**CFILB reference monitor implementation:** cfilbLibs/

**Reference monitor instrumentation:** llvm-project/clang/lib/CodeGen/CGCall.cpp

**Clang libtool to prepare the source:** llvm-project/clang/tools/clangCodePrep/

**Intel pin dynamic CFG generator:** dCFG/

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

Step 5: Build the binary from the step 4 bitcode.

Step 6: Extract symbol table from the elf binary.

Step 7: Execute the binary with seed input using intel pin tool to generate dynamic CFG.

Step 8: Run a python script to apply the adaptive algorithm.

Step 9: Run another LLVM Pass to instrument the adaptive dynamic CFG table in the bitcode.

Step 10: Build the final binary from the step 9 bitcode. The binary will be named as: benchmarkname_cfg

## Installation Guideline
1. Git clone the project:
```text
git clone git@github.com:mustakcsecuet/CFI-LB.git
```
***Note: You can skip step 2, 3, 4, 6, and 7 if you have already configured Gold plugin for another compiler.***

2. Install required library for Gold plugin:
```text
sudo apt-get install linux-headers-$(uname -r) git g++ gcc cmake bash csh gawk automake libtool bison flex libncurses5-dev
# Check 'makeinfo -v'. If 'makeinfo' does not exist
sudo apt-get install apt-file texinfo texi2html
sudo apt-file update
sudo apt-file search makeinfo
```

3. Download binutils source code:
```text
cd ~
git clone --depth 1 git://sourceware.org/git/binutils-gdb.git binutils
```

4. Build binutils:
```text
mkdir build
cd build
../binutils/configure --enable-gold --enable-plugins --disable-werror
make
```

5. Build the compiler (use the binutils directory if you already have one):
```text
cd ../llvm-project
mkdir build
cmake -DLLVM_BINUTILS_INCDIR="path_to_binutils/include"  -DLLVM_ENABLE_PROJECTS=clang -G "Unix Makefiles" ../llvm
make -j8
sudo make install
```

6. Backup ar, nm, ld and ranlib:
```text
cd ~
mkdir backup
cd /usr/bin/
cp ar ~/backup/
cp nm ~/backup/
cp ld ~/backup/
cp ranlib ~/backup/
```

7. Replace ar, nm, ld and ranlib:
```text
cd /usr/bin/
sudo cp ~/build/binutils/ar ./
sudo rm nm
sudo cp ~/build/binutils/nm-new ./nm
sudo cp ~/build/binutils/ranlib ./
sudo cp ~/build/gold/ld-new ./ld
```

8. install LLVMgold.so to /usr/lib/bfd-plugins:
```text
cd /usr/lib
sudo mkdir bfd-plugins
cd bfd-plugins
sudo cp /path_to_llvm_project_build/lib/LLVMgold.so ./
sudo cp /path_to_llvm_project_build/lib/libLTO.* ./
```

9. Download intel-pin-3.5 source:
```text
cd /path_to_project_directory/
wget http://software.intel.com/sites/landingpage/pintool/downloads/pin-3.5-97503-gac534ca30-gcc-linux.tar.gz
tar -xvzf pin-3.5-97503-gac534ca30-gcc-linux.tar.gz
rm pin-3.5-97503-gac534ca30-gcc-linux.tar.gz
mv pin-3.5-97503-gac534ca30-gcc-linux intel-pin
```

10. Build the dynamic CFG generation pin:
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
