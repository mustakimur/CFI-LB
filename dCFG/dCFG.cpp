/*
 * CFI-LB: Adaptive Call-site Sensitive Control Flow Integrity
 * Authors: Mustakimur Khandaker (Florida State University)
 * Abu Naser (Florida State University)
 * Wenqing Liu (Florida State University)
 * Zhi Wang (Florida State University)
 * Yajin Zhou (Zhejiang University)
 * Yueqiang Chen (Baidu X-lab)
 */

/*
 * This is a intel-pin based tool to generate dynamic CFG based on
 * valid inputs for the concrete execution. The CFG will write (append)
 * in cfilb_cfg.bin at target binary directory. It has dependency to
 * extracted elf information from utils/extract.py; so the python
 * script should run before it.
 */

/*
 * This will instrument every call and return instruction to maintain a
 * similar call stack as the system; only at indirect call point, it will
 * check its call stack to retrieve context-based CFG and store it in memory.
 * It will dump the CFG collection in the file at the end.
 */

#include "pin.H"
#include "stack.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>

#define DEBUG false // DEBUG flag

// maximum byte to store dynamic cfg in memory
#define MAX_DATA 5000000
#define HASH_KEY_RANGE 1000000

string cfgpath;

ADDRINT monitorADDR;

struct EventIndiCall {
  unsigned long long iaddr;  // call-point (indirect call)
  unsigned long long taddr;  // call-target (function entry)
  unsigned long long raddr3; // call-site with level 3
  unsigned long long raddr2; // call-site with level 2
  unsigned long long raddr1; // call-site with level 1
};

// cfg collection in memory
struct EventIndiCall iCollection[MAX_DATA];
int iCollectionCounter = 0;
int lastSavedCounter = 0;

// hash based test table
struct HashItem {
  unsigned long long call_site[3];
  unsigned long long point;
  unsigned long long target;
  struct HashItem *next;
};
struct HashItem *CFILB_HASH_TABLE[HASH_KEY_RANGE] = {NULL};

// reference monitor at level 3
int check_hash(unsigned long point, unsigned long target, unsigned long site1,
               unsigned long site2, unsigned long site3) {
  unsigned long hashIndex =
      (point ^ target ^ site1 ^ site2 ^ site3) % HASH_KEY_RANGE;
  if (CFILB_HASH_TABLE[hashIndex] != NULL) {
    struct HashItem *temp = CFILB_HASH_TABLE[hashIndex];
    while (temp != NULL) {
      if (temp->point == point && temp->target == target &&
          temp->call_site[0] == site1 && temp->call_site[1] == site2 &&
          temp->call_site[2] == site3) {
        return 1;
      }
      temp = temp->next;
    }
  }
  return 0;
}

int create_hash(unsigned long point, unsigned long target, unsigned long site1,
                unsigned long site2, unsigned long site3) {
  unsigned long hashIndex =
      (point ^ target ^ site1 ^ site2 ^ site3) % HASH_KEY_RANGE;
  if (!check_hash(point, target, site1, site2, site3)) {
    struct HashItem *item = (struct HashItem *)malloc(sizeof(struct HashItem));
    item->call_site[0] = site1;
    item->call_site[1] = site2;
    item->call_site[2] = site3;
    item->point = point;
    item->target = target;
    item->next = NULL;

    if (CFILB_HASH_TABLE[hashIndex] == NULL) {
      CFILB_HASH_TABLE[hashIndex] = item;
    } else {
      struct HashItem *temp = CFILB_HASH_TABLE[hashIndex];
      while (temp->next != NULL) {
        temp = temp->next;
      }
      temp->next = item;
    }
    return 1;
  } else {
    return 0;
  }
}

// copy the cfg to cfg collection in memory
void copyToCollect(unsigned long point, unsigned long target,
                   unsigned long site1, unsigned long site2,
                   unsigned long site3) {
  iCollection[iCollectionCounter].iaddr = point;
  iCollection[iCollectionCounter].taddr = target;
  iCollection[iCollectionCounter].raddr3 = site3;
  iCollection[iCollectionCounter].raddr2 = site2;
  iCollection[iCollectionCounter].raddr1 = site1;
}

// dump cfg to the file
void write_to_callfd() {
  ofstream callfd;
  callfd.open(cfgpath.c_str(), std::ofstream::app);
  for (int i = lastSavedCounter; i < iCollectionCounter; i++) {
    callfd << iCollection[i].iaddr << "\t" << iCollection[i].taddr << "\t"
           << iCollection[i].raddr1 << "\t" << iCollection[i].raddr2 << "\t"
           << iCollection[i].raddr3 << "\n";
  }
  lastSavedCounter = iCollectionCounter;
  callfd.close();
}

VOID dump_cfg(CONTEXT *ctxt, void *ip) {
  unsigned long point = (unsigned long)PIN_GetContextReg(ctxt, REG_RDI);
  unsigned long target = (unsigned long)PIN_GetContextReg(ctxt, REG_RSI);
  unsigned long site1 = (unsigned long)PIN_GetContextReg(ctxt, REG_RDX);
  unsigned long site2 = (unsigned long)PIN_GetContextReg(ctxt, REG_RCX);
  unsigned long site3 = (unsigned long)PIN_GetContextReg(ctxt, REG_R8);

  if (create_hash(point, target, site1, site2, site3)) {
    // a new cfg
    copyToCollect(point, target, site1, site2, site3);
    iCollectionCounter++;
  }

  if (iCollectionCounter % 20 == 0) {
    // infrequently dump in the file after discovering 20 cfg
    write_to_callfd();
  }
}

VOID Instruction(INS ins, VOID *v) {
  if (INS_Address(ins) == monitorADDR) {
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)dump_cfg, IARG_CONTEXT,
                   IARG_INST_PTR, IARG_END);
  }
}

VOID Fini(INT32 code, VOID *v) { write_to_callfd(); }

INT32 Usage() {
  PIN_ERROR("Intel pin based dynamic CFG generation tool" +
            KNOB_BASE::StringKnobSummary() + "\n");
  return -1;
}

// ./pin -t obj-intel64/d-analysis.so -- target_binary_path valid_input_path
int main(int argc, char *argv[]) {
  if (PIN_Init(argc, argv)) {
    return Usage();
  }

  // following information should be generated before from utils/extract.py
  string elfpath = "elf_extract.bin";
  ifstream binprocfd;
  binprocfd.open(elfpath.c_str());
  string name;
  unsigned long long addr;
  unsigned int size;
  if (binprocfd.is_open()) {
    while (binprocfd >> name >> addr >> size) {
      if (name == "cd_cfg_monitor") {
        monitorADDR = addr;
      }
    }
  }
  binprocfd.close();

  // store existing cfg in memory
  cfgpath = "cfilb_cfg.bin";
  ifstream initcallfd;
  initcallfd.open(cfgpath.c_str());
  if (initcallfd.is_open()) {
    while (initcallfd >> iCollection[iCollectionCounter].iaddr >>
           iCollection[iCollectionCounter].taddr >>
           iCollection[iCollectionCounter].raddr1 >>
           iCollection[iCollectionCounter].raddr2 >>
           iCollection[iCollectionCounter].raddr3) {
      create_hash(iCollection[iCollectionCounter].iaddr,
                  iCollection[iCollectionCounter].taddr,
                  iCollection[iCollectionCounter].raddr1,
                  iCollection[iCollectionCounter].raddr2,
                  iCollection[iCollectionCounter].raddr3);
      iCollectionCounter++;
    }
  }
  initcallfd.close();
  lastSavedCounter = iCollectionCounter;

  PIN_InitSymbols();
  INS_AddInstrumentFunction(Instruction, 0);
  PIN_AddFiniFunction(Fini, 0);
  PIN_StartProgram();

  return 0;
}
