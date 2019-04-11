/*
 * CFI-LB: Adaptive Call-site Sensitive Control Flow Integrity
 * Authors: Mustakimur Khandaker (Florida State University)
 */

#include "pin.H"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

#define DEBUG false

// architecture based data type size
#define SCHAR sizeof(char)
#define SUCHAR sizeof(unsigned char)
#define SUDEC sizeof(unsigned int)
#define SULONG sizeof(unsigned long long)
#define SBOOL sizeof(bool)

// maximum buffer size to cache information
#define MAX_DUMP_DATA 100000

using namespace std;

// we dump 5 kind of information
// (function_entry_block_entry, register_to_memory_write,
// memory_to_register_write, mov_rsp_to_rbp, function_entry_block_exit)
enum tags {
  FUNC_ENTRY = '1',
  REG_MEM_WRITE,
  REG_MEM_READ,
  REG_RSP_RBP_MOV,
  GLB_MEM,
  FUNC_EXIT
};

// file descriptor of dumped binary file
FILE *dumpFile;

// buffer to cache information
char dumpdata[MAX_DUMP_DATA];
int writtenDumpData;

map<unsigned long long, unsigned int> glbMemMap;
map<unsigned long long, bool> visited_func;

map<unsigned long long, bool> usr_func;

map<unsigned long long, bool> executed_func;

// structure for function_entry_block_entry
struct EFunc {
  char tag;
  unsigned long long faddr;
};

// structure for move_rsp_rbp
struct EReg {
  char tag;
  unsigned int size;
  unsigned char *val;
};

// structure for register_to_memory_write
struct EMem {
  char tag;
  unsigned long long iaddr;
  unsigned long long maddr;
  unsigned int msize;
  bool isSymbolic;
  unsigned char *mval;
};

// convert hex string to unsigned long long
unsigned long long stoull(char *s) {
  unsigned long long dec_val = 0;
  unsigned long long base = 1;
  int size = strlen(s);

  for (int i = size - 1; i >= 0; i--) {
    if (s[i] >= '0' && s[i] <= '9') {
      dec_val += (s[i] - '0') * base;
      base = base * 16;
    } else if (s[i] >= 'a' && s[i] <= 'f') {
      dec_val += (s[i] - 'a' + 10) * base;
      base = base * 16;
    }
  }

  return dec_val;
}

// dump to disk of a function data
VOID write_to_disk() {
  if (DEBUG)
    cout << "writing to disk ..." << endl;
  fwrite(dumpdata, 1, writtenDumpData, dumpFile);
  writtenDumpData = 0;
}

// call to stoull to convert hex string to unsigned long long with proper
// formatting
unsigned long long getData(unsigned int size, unsigned char *wval) {
  unsigned long long data;
  stringstream ss;
  ss << hex;
  for (int i = size - 1; i >= 0; i--) {
    if (int(wval[i]) < 16) {
      ss << "0";
    }
    ss << int(wval[i]);
  }
  string str = ss.str();
  char s[100];
  strcpy(s, str.c_str());
  data = stoull(s);
  return data;
}

void bufferedEventMem(EMem *ev) {
  memcpy(dumpdata + writtenDumpData, &(ev->tag), SCHAR);
  writtenDumpData += SCHAR;
  memcpy(dumpdata + writtenDumpData, &(ev->iaddr), SULONG);
  writtenDumpData += SULONG;
  memcpy(dumpdata + writtenDumpData, &(ev->maddr), SULONG);
  writtenDumpData += SULONG;
  memcpy(dumpdata + writtenDumpData, &(ev->msize), SUDEC);
  writtenDumpData += SUDEC;
  memcpy(dumpdata + writtenDumpData, &(ev->isSymbolic), SBOOL);
  writtenDumpData += SBOOL;
  /*if (ev->msize > 100) {
    ev->msize = 100;
  }*/
  memcpy(dumpdata + writtenDumpData, (ev->mval), ev->msize);
  writtenDumpData += ev->msize;
}

bool shouldSymbolic(unsigned int size, unsigned char *wval, int c) {
  if (DEBUG)
    cout << "[DSC] begin shouldSymbolic ..." << endl;
  if (c == 0) {
    if (DEBUG)
      cout << "[DSC] end shouldSymbolic as false(0)..." << endl;
    return false;
  }
  unsigned long long data;
  data = getData(size, wval);

  if (data == 0 || usr_func.find(data) != usr_func.end()) {
    if (DEBUG)
      cout << "[DSC] end shouldSymbolic as false(1)..." << endl;
    return false;
  }

  if (DEBUG)
    cout << "[shouldSymbolic] Memory address [int] " << data << " testing ...."
         << endl;

  OS_MEMORY_AT_ADDR_INFORMATION *info = new OS_MEMORY_AT_ADDR_INFORMATION();
  if (OS_QueryMemory(PIN_GetPid(), (VOID *)data, info).generic_err ==
      OS_RETURN_CODE_NO_ERROR) {
    if (info->Protection != 5) {
      if (DEBUG)
        cout << "[shouldSymbolic] Memory address [int] " << data << " passed."
             << endl;

      EMem *ev = new EMem;
      ev->tag = REG_MEM_READ;
      ev->maddr = (unsigned long long)data;
      ev->msize = (unsigned int)8;
      ev->iaddr = (unsigned long long)0x0;

      ev->mval = (unsigned char *)malloc(ev->msize * SUCHAR);
      PIN_SafeCopy(ev->mval, (VOID *)ev->maddr, ev->msize);

      if (getData(ev->msize, ev->mval) == data) {
        return false;
      }

      if (shouldSymbolic(ev->msize, ev->mval, --c)) {
        ev->isSymbolic = false;
      }

      bufferedEventMem(ev);

      ev->mval = NULL;
      free(ev->mval);
      delete ev;
      if (DEBUG)
        cout << "[DSC] end shouldSymbolic as true..." << endl;
      return true;
    }
  }
  if (DEBUG)
    cout << "[DSC] end shouldSymbolic as false(2)..." << endl;
  return false;
}

void glbMemSnapShot() {
  if (DEBUG)
    cout << "[DSC] begin glbMemSnapShot ..." << endl;
  EMem *ev = new EMem;
  ev->tag = GLB_MEM;
  ev->mval = (unsigned char *)malloc(1000 * SUCHAR);
  map<unsigned long long, unsigned int>::iterator it;
  for (it = glbMemMap.begin(); it != glbMemMap.end(); ++it) {
    ev->maddr = (unsigned long long)it->first;
    ev->msize = (unsigned int)it->second;

    if (ev->msize >= 1000) {
      ev->msize = 1000;
    }

    PIN_SafeCopy(ev->mval, (VOID *)ev->maddr, ev->msize);

    ev->isSymbolic = true;
    ev->iaddr = 0x0;

    // do check if memory address
    if (ev->msize <= 8) {
      if (shouldSymbolic(ev->msize, ev->mval, 4)) {
        ev->isSymbolic = false;
      }
    }

    // store in memory (memory_dump_tag, memory_address, content_size,
    // memory_content)
    bufferedEventMem(ev);
  }
  ev->mval = NULL;
  free(ev->mval);
  delete ev;
  if (DEBUG)
    cout << "[DSC] end glbMemSnapShot ..." << endl;
}

// dump register information from function entry block in register read
// instruction
VOID dumpRegEvent(VOID *ip, REG reg, CONTEXT *ctxt) {
  if (DEBUG) {
    cout << "Instrution Address: 0x" << hex << (unsigned long long)ip << dec
         << endl;
    cout << "Read from register: " << REG_StringShort(reg) << endl;
    cout << "Register size: " << REG_Size(reg) << endl;
    cout << "Content: ";
  }

  EReg *ev = new EReg;

  // register size (in bytes)
  ev->size = REG_Size(reg);

  // register content retrieve
  PIN_REGISTER regval;
  PIN_GetContextRegval(ctxt, reg, reinterpret_cast<UINT8 *>(&regval));

  ev->val = (unsigned char *)malloc(ev->size * SUCHAR);

  // prepare the register read event
  for (unsigned int i = 0; i < ev->size; i++) {
    if (DEBUG)
      cout << "0x" << hex << (int)regval.byte[i] << " ";
    ev->val[i] = regval.byte[i];
  }
  if (DEBUG)
    cout << endl;
  ev->tag = REG_RSP_RBP_MOV;

  // store in register (register_dump_tag, register_id, register_size,
  // register_content)
  memcpy(dumpdata + writtenDumpData, &(ev->tag), SCHAR);
  writtenDumpData += SCHAR;
  memcpy(dumpdata + writtenDumpData, &(ev->size), SUDEC);
  writtenDumpData += SUDEC;
  memcpy(dumpdata + writtenDumpData, (ev->val), ev->size);
  writtenDumpData += ev->size;

  free(ev->val);
  delete ev;
}

// dump memory information from function entry block in memory read instruction
VOID RecordMemRead(VOID *addr, UINT32 size, VOID *ip) {
  if (DEBUG) {
    cout << "Memory read from 0x" << hex << (unsigned long long)addr
         << "of size " << dec << (unsigned int)size << endl;
    cout << "Content: ";
  }

  EMem *ev = new EMem;
  ev->tag = REG_MEM_READ;
  ev->maddr = (unsigned long long)addr;
  ev->msize = (unsigned int)size;
  ev->iaddr = (unsigned long long)ip;

  ev->mval = (unsigned char *)malloc(ev->msize * SUCHAR);
  PIN_SafeCopy(ev->mval, (VOID *)ev->maddr, ev->msize);

  ev->isSymbolic = true;
  // do check if memory address
  if (ev->msize <= 8) {
    if (shouldSymbolic(ev->msize, ev->mval, 4)) {
      ev->isSymbolic = false;
    }
  }

  // store in memory (memory_dump_tag, memory_address, content_size,
  // memory_content)
  bufferedEventMem(ev);
  ev->mval = NULL;
  free(ev->mval);
  delete ev;
}

static VOID *ReadIPAddr;
static VOID *ReadAddr;
static INT32 ReadSize;

// instrument before instruction is being taken or fallen
static VOID RecordReadAddrSize(VOID *addr, INT32 size, INS ins) {
  ReadAddr = addr;
  ReadSize = size;
}

// instrument if instruction is being taken or fallen
static VOID RecordMemReadIM(VOID *ip) {
  if (DEBUG) {
    cout << "Memory read instruction address: 0x" << hex
         << (unsigned long long)(ip) << dec << endl;
  }
  ReadIPAddr = ip;
  RecordMemRead(ReadAddr, ReadSize, ReadIPAddr);
}

// Commented to do initial benchmark
// dump memory write instruction information
VOID RecordMemWrite(VOID *addr, UINT32 size, VOID *ip) {
  if (DEBUG) {
    cout << "Memory read from 0x" << hex << (unsigned long long)addr
         << "of size " << dec << (unsigned int)size << endl;
    cout << "Content: ";
  }

  EMem *ev = new EMem;
  ev->tag = REG_MEM_WRITE;
  ev->maddr = (unsigned long long)addr;
  ev->msize = (unsigned int)size;
  ev->iaddr = (unsigned long long)ip;

  ev->mval = (unsigned char *)malloc(ev->msize * SUCHAR);
  PIN_SafeCopy(ev->mval, (VOID *)ev->maddr, ev->msize);

  ev->isSymbolic = true;
  // do check if memory address
  if (ev->msize <= 8) {
    if (shouldSymbolic(ev->msize, ev->mval, 4)) {
      ev->isSymbolic = false;
    }
  }

  // store in memory (memory_dump_tag, memory_address, content_size,
  // memory_content)
  bufferedEventMem(ev);
  ev->mval = NULL;
  free(ev->mval);
  delete ev;
}

static VOID *WriteIPAddr;
static VOID *WriteAddr;
static INT32 WriteSize;

static VOID RecordWriteAddrSize(VOID *addr, INT32 size) {
  WriteAddr = addr;
  WriteSize = size;
}

static VOID RecordMemWriteIM(VOID *ip) {
  if (DEBUG) {
    cout << "Memory write instruction address: 0x" << hex
         << (unsigned long long)(ip) << dec << endl;
  }
  WriteIPAddr = ip;
  RecordMemWrite(WriteAddr, WriteSize, WriteIPAddr);
}

// dump function entry block entry instruction information
VOID funcEntryBlockEntry(VOID *ip) {
  if (DEBUG) {
    cout << endl
         << " Entry: 0x" << hex << (unsigned long long)(ip) << dec << endl;
  }

  // prepare function entry event
  EFunc *ev = new EFunc;
  ev->tag = FUNC_ENTRY;
  ev->faddr = (unsigned long long)ip;

  // store in memory (function_entry_tag, function_entry_address)
  memcpy(dumpdata + writtenDumpData, &(ev->tag), SCHAR);
  writtenDumpData += SCHAR;
  memcpy(dumpdata + writtenDumpData, &(ev->faddr), SULONG);
  writtenDumpData += SULONG;

  delete ev;

  glbMemSnapShot();
}

// dump function entry block exit instruction information
VOID funcEntryBlockExit(VOID *ip) {
  if (DEBUG) {
    cout << "Exit: 0x" << hex << (unsigned long long)(ip) << dec << endl;
  }

  // prepare function entry block exit event
  EFunc *ev = new EFunc;
  ev->tag = FUNC_EXIT;
  ev->faddr = (unsigned long long)ip;

  memcpy(dumpdata + writtenDumpData, &(ev->tag), SCHAR);
  writtenDumpData += SCHAR;
  memcpy(dumpdata + writtenDumpData, &(ev->faddr), SULONG);
  writtenDumpData += SULONG;

  if (executed_func.find(ev->faddr) == executed_func.end()) {
    executed_func[ev->faddr] = true;
    write_to_disk();
  } else {
    writtenDumpData = 0;
  }
  delete ev;
  PIN_RemoveInstrumentation();
}

// Routine Function: mapping function_entry_address with function_name
VOID Routine(RTN proc, VOID *v) {
  if (visited_func.find(RTN_Address(proc)) == visited_func.end())
    visited_func[RTN_Address(proc)] = false;
}

// if the trace starts from a function entry, then use it
VOID Trace(TRACE trace, VOID *v) {
  // Trace Basic Block Head = > Basic Block Instruction Head
  // == could be Function_Entry_Block_Entry_Instruction
  BBL bbl_head = TRACE_BblHead(trace);
  INS ins_head = BBL_InsHead(bbl_head);
  INS ins_tail = BBL_InsTail(bbl_head);

  // verify if instruction address is a source function entry address
  // and also never being visited
  unsigned long long address = (unsigned long long)INS_Address(ins_head);
  if ((usr_func.find(address) != usr_func.end()) && !visited_func[address]) {
    // instrument function block entry and exit instructions
    INS_InsertCall(ins_head, IPOINT_BEFORE, (AFUNPTR)funcEntryBlockEntry,
                   IARG_INST_PTR, IARG_END);

    if (INS_IsBranch(ins_tail) && INS_Address(INS_Prev(ins_tail)) != 0) {
      INS ins_prev = INS_Prev(ins_tail);
      INS_InsertCall(ins_prev, IPOINT_BEFORE, (AFUNPTR)funcEntryBlockExit,
                     IARG_INST_PTR, IARG_END);
    } else {
      INS_InsertCall(ins_tail, IPOINT_BEFORE, (AFUNPTR)funcEntryBlockExit,
                     IARG_INST_PTR, IARG_END);
    }

    // instrument all register and memory read instructions inside function
    // entry block
    for (INS ins = ins_head; ins != ins_tail; ins = INS_Next(ins)) {
      if (INS_Opcode(ins) == XED_ICLASS_MOV && INS_RegRContain(ins, REG_RSP) &&
          INS_RegWContain(ins, REG_RBP)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)dumpRegEvent, IARG_INST_PTR,
                       IARG_UINT32, REG(INS_OperandReg(ins, 1)), IARG_CONTEXT,
                       IARG_END);
      }

      // instrument register read instruction
      if (INS_Opcode(ins) == XED_ICLASS_MOV && INS_OperandIsReg(ins, 1) &&
          INS_OperandIsMemory(ins, 0)) {
        INS_InsertPredicatedCall(ins, IPOINT_BEFORE,
                                 (AFUNPTR)RecordWriteAddrSize,
                                 IARG_MEMORYWRITE_EA, IARG_MEMORYWRITE_SIZE,
                                 IARG_INST_PTR, IARG_END);

        if (INS_HasFallThrough(ins)) {
          INS_InsertCall(ins, IPOINT_AFTER, (AFUNPTR)RecordMemWriteIM,
                         IARG_INST_PTR, IARG_END);
        }
        if (INS_IsBranchOrCall(ins)) {
          INS_InsertCall(ins, IPOINT_TAKEN_BRANCH, (AFUNPTR)RecordMemWriteIM,
                         IARG_INST_PTR, IARG_END);
        }
      }

      // instrument memory read instruction (only memory to register is valid)
      if (INS_Opcode(ins) == XED_ICLASS_MOV && INS_OperandIsMemory(ins, 1) &&
          INS_OperandIsReg(ins, 0)) {
        INS_InsertPredicatedCall(
            ins, IPOINT_BEFORE, (AFUNPTR)RecordReadAddrSize, IARG_MEMORYREAD_EA,
            IARG_MEMORYREAD_SIZE, IARG_INST_PTR, IARG_END);

        if (INS_HasFallThrough(ins)) {
          INS_InsertCall(ins, IPOINT_AFTER, (AFUNPTR)RecordMemReadIM,
                         IARG_INST_PTR, IARG_END);
        }
        if (INS_IsBranchOrCall(ins)) {
          INS_InsertCall(ins, IPOINT_TAKEN_BRANCH, (AFUNPTR)RecordMemReadIM,
                         IARG_INST_PTR, IARG_END);
        }
      }
    }

    visited_func[address] = true;
  }
}

// instrument write to heap memory // 0x6c5580 // 400b3e
void loadGlobals() {
  string res = "glb_snapshot.bin";
  ifstream glbFile(res.c_str());

  unsigned long long addr;
  unsigned int size;

  if (glbFile.is_open()) {
    while (glbFile >> addr >> size) {
      glbMemMap[addr] = size;
    }
  }

  glbFile.close();
}

// load visited functions information
void loadVisited() {
  string res = "tracked_func.bin";
  ifstream trackedFile(res.c_str());

  unsigned long long addr;

  if (trackedFile.is_open()) {
    while (trackedFile >> addr) {
      visited_func[addr] = true;
    }
  }

  trackedFile.close();
}

void loadUsrFunc() {
  string res = "elf_extract.bin";
  ifstream binprocfd(res.c_str());
  string name;
  unsigned int size;
  unsigned long long addr;
  if (binprocfd.is_open()) {
    while (binprocfd >> name >> addr >> size) {
      usr_func[addr] = true;
    }
  }
  binprocfd.close();
}

// write the visited function's entry address
void write_to_tracked() {
  string res = "tracked_func.bin";
  ofstream trackedFile(res.c_str());

  map<unsigned long long, bool>::iterator it;
  for (it = visited_func.begin(); it != visited_func.end(); ++it) {
    if (it->second)
      trackedFile << it->first << "\n";
  }

  trackedFile.close();
}

// Execution Finish: close opened files and write back remaining information
VOID Fini(INT32 code, VOID *v) {
  write_to_tracked();
  fclose(dumpFile);
}

// usage error warning [command line parsing error]
INT32 Usage() {
  PIN_ERROR(
      "This will generate the functional seed data on funcSeedRepo.bin\n" +
      KNOB_BASE::StringKnobSummary() + "\n");
  return -1;
}

// Intel pin tool to dump initial function data for symbolic code coverage
int main(int argc, char *argv[]) {
  if (PIN_Init(argc, argv)) {
    return Usage();
  }

  loadUsrFunc();          // load source function list in a map
  loadVisited();          // load already tracked function list in a map
  loadGlobals();          // load global memory information in a map
  // funcSeedRepo.bin is a binary file
  writtenDumpData = 0;
  string dumppath = "funcSeedRepo.bin";
  dumpFile = fopen(dumppath.c_str(), "ab");

  PIN_InitSymbols(); // initialize intel pin tool

  // Instrument every function [just to help in keep tracking visited function]
  RTN_AddInstrumentFunction(Routine, 0);

  // A function should start a new trace, so we look into it
  TRACE_AddInstrumentFunction(Trace, 0);

  // intel pin job done
  PIN_AddFiniFunction(Fini, 0);

  // starting intel pin tool
  PIN_StartProgram();

  return 0;
}
