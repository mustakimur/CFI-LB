echo "CFILB Project Directory:"
read projectDirectory
if [ ${projectDirectory:0:1} == '~' ]
then
  pDir="$HOME""${projectDirectory:1}"
fi 
pDir=`realpath "$pDir"`

echo "Source Directory:"
read sourceDirectory
if [ ${sourceDirectory:0:1} == '~' ]
then
  sDir="$HOME""${sourceDirectory:1}"
fi 
sDir=`realpath "$sDir"`

echo "Binary Name:"
read binaryName
#binaryName="hmmer"

echo "Seed Directory:"
read inputDirectory
if [ ${inputDirectory:0:1} == '~' ]
then
  iDir="$HOME""${inputDirectory:1}"
fi 
iDir=`realpath "$iDir"`

cd $sDir

# refresh the target source
make clean
rm *.c *.h *.bc *.ll *.bin *.log *.asm
cp ../../src/*.c .
cp ../../src/*.h .

# copy the CFILB libs
cp "$pDir""/cfiLibs/libs/cfilb.c" .
#cp "$pDir""/cfiLibs/libs/libcfilb.a" .
cp "$pDir""/cfiLibs/libs/cfilb.h" .

# prepare the code
for f in *.c; do
  "$pDir""/llvm-project/build/bin/cfilb-prep" -extra-arg="-DSPEC_CPU -DSPEC_CPU_LP64" "$f" -- > "${f%.c}.tmp"
  mv "${f%.c}.tmp" "$f";
done

# build the code
make

# deassemble to .ll
"$pDir""/llvm-project/build/bin/llvm-dis" "$binaryName"".0.4.opt.bc"

# static CFG and instrument
"$pDir""/llvm-project/build/bin/opt" -load "$pDir""/llvm-project/build/lib/LLVMCiCfi.so" -llvm-ci-cfi < "$binaryName"".0.4.opt.bc" > "$binaryName"".0.4.scfg.bc"

# deassemble to .ll
"$pDir""/llvm-project/build/bin/llvm-dis" "$binaryName"".0.4.scfg.bc"

# build the binary
"$pDir""/llvm-project/build/bin/llc" -filetype=obj "$binaryName"".0.4.scfg.bc"
"$pDir""/llvm-project/build/bin/clang++" "$binaryName"".0.4.scfg.o" -o "$binaryName""_scfg"

cp "$binaryName""_scfg" $iDir
cd $iDir
python "$pDir""/utils/extract.py" "$binaryName""_scfg"
# modify the next line with seeds to execute (e.g. 456.hmmer case). You can add multiple seeds to execute multiple time
#"$pDir""/intel-pin/pin" -t "$pDir""/dCFG/obj-intel64/dCFG.so" -- ./"$binaryName""_scfg" nph3.hmm swiss41
python "$pDir""/utils/filter.py"
cd $sDir

"$pDir""/llvm-project/build/bin/opt" -load "$pDir""/llvm-project/build/lib/LLVMInstCFG.so" -llvm-inst-cfg -DIR_PATH="$iDir" < "$binaryName"".0.4.scfg.bc" > "$binaryName"".0.4.cfg.bc"

# deassemble to .ll
"$pDir""/llvm-project/build/bin/llvm-dis" "$binaryName"".0.4.cfg.bc"

# build the binary
"$pDir""/llvm-project/build/bin/llc" -filetype=obj "$binaryName"".0.4.cfg.bc"
"$pDir""/llvm-project/build/bin/clang++" "$binaryName"".0.4.cfg.o" -o "$binaryName""_cfg"

