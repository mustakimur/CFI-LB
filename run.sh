pDir=$CFILB_PATH

echo "Target Program Source Directory:"
echo "e.g. ~/spec/benchspec/CPU2006/456.hmmer/build/build_base_amd64-m64-softbound-nn.0000"
read sourceDirectory
if [ ${sourceDirectory:0:1} == '~' ]
then
  sDir="$HOME""${sourceDirectory:1}"
fi 
sDir=`realpath "$sDir"`

echo "Binary Name (e.g. hmmer for 456.hmmer benchmark):"
read binaryName

echo "Will you like to use concolic process (only C int benchmark)? y/n"
read isConcolic

cd $sDir

# refresh the target source
make clean
rm *.c *.h *.bc *.ll *.bin *.log *.asm
rm -r binary
cp ../../src/*.c .
cp ../../src/*.h .

# copy the CFILB libs
cp "$pDir""/cfiLibs/libs/cfilb.c" .
cp "$pDir""/cfiLibs/libs/cfilb.h" .

# prepare the code
for f in *.c; do
  "$pDir""/llvm-project/build/bin/cfilb-prep" -extra-arg="-DSPEC_CPU -DSPEC_CPU_LP64" "$f" -- > "${f%.c}.tmp"
  mv "${f%.c}.tmp" "$f";
done

# build the code
make

# deassemble to .ll (optional)
# "$pDir""/llvm-project/build/bin/llvm-dis" "$binaryName"".0.4.opt.bc"

# static CFG and instrument
"$pDir""/llvm-project/build/bin/opt" -load "$pDir""/llvm-project/build/lib/LLVMCiCfi.so" -llvm-ci-cfi < "$binaryName"".0.4.opt.bc" > "$binaryName"".0.4.scfg.bc"

# deassemble to .ll (optional)
# "$pDir""/llvm-project/build/bin/llvm-dis" "$binaryName"".0.4.scfg.bc"

# build the binary
"$pDir""/llvm-project/build/bin/llc" -filetype=obj "$binaryName"".0.4.scfg.bc"
"$pDir""/llvm-project/build/bin/clang++" "$binaryName"".0.4.scfg.o" -o "$binaryName""_scfg"

mkdir binary
cp "$binaryName""_scfg" binary
cd binary

echo "please copy your seed files (if you have any) to following directory"
pwd

python "$pDir""/utils/extract.py" "$binaryName""_scfg"

echo "give the command line arguments for each seed (if any); q indicates no more seed."

while read line; do
	if [[ $line = q ]]; then
		break
	fi
  "$pDir""/intel-pin/pin" -t "$pDir""/dCFG/obj-intel64/dCFG.so" -- ./"$binaryName""_scfg" $line
  if [[ $isConcolic = y ]]; then
    echo 0 | sudo tee /proc/sys/kernel/randomize_va_space
    "$pDir""/intel-pin/pin" -t "$pDir""/cHelper/obj-intel64/sym-dump.so" -- ./"$binaryName""_scfg" $line
    echo 2 | sudo tee /proc/sys/kernel/randomize_va_space
  fi
  echo "Next Seed: "
done

if [[ $isConcolic = y ]]; then
  python "$pDir""/utils/symHelper.py" "$binaryName""_scfg"
  input="sym_monitor.bin"
  while read var1 var2
  do
    echo "sym-engine is running for " "$var1" "$var2"
    "$pDir""/cCFG/triton/sym_engine/build/src/examples/symCFG/symEmulator" "$binaryName""_scfg" "$var1" "$var2"
  done < "$input"
fi

python "$pDir""/utils/filter.py"
cd $sDir

"$pDir""/llvm-project/build/bin/opt" -load "$pDir""/llvm-project/build/lib/LLVMInstCFG.so" -llvm-inst-cfg -DIR_PATH="$sDir""/binary" < "$binaryName"".0.4.scfg.bc" > "$binaryName"".0.4.cfg.bc"

# deassemble to .ll (option)
# "$pDir""/llvm-project/build/bin/llvm-dis" "$binaryName"".0.4.cfg.bc"

# build the binary
"$pDir""/llvm-project/build/bin/llc" -filetype=obj "$binaryName"".0.4.cfg.bc"
"$pDir""/llvm-project/build/bin/clang++" "$binaryName"".0.4.cfg.o" -o "$binaryName""_cfg"

cp "$binaryName""_cfg" binary