/*
 * CFI-LB: Adaptive Call-site Sensitive Control Flow Integrity
 * Authors: Mustakimur Khandaker (Florida State University)
 */
#ifndef IMODEL_H
#define IMODEL_H
#include <fstream>
#include <iostream>
#include <triton/api.hpp>
#include <triton/elf.hpp>
#include <triton/x86Specifications.hpp>
#include "simglibc.h"

// Triton code base greatly depends on namespace properties
using namespace std;
using namespace triton;
using namespace triton::arch;
using namespace triton::modes;
using namespace triton::format;
using namespace triton::format::elf;

#define MAX_DATA 50000

#define SUCHAR sizeof(unsigned char)
#define SUDEC sizeof(unsigned int)
#define SULONG sizeof(unsigned long long)
#define SBOOL sizeof(bool)

#define S_CFG 'S'  // represents symbolic cfg

enum tags {
  FUNC_ENTRY = '1',
  REG_MEM_WRITE,
  REG_MEM_READ,
  REG_RSP_RBP_MOV,
  GLB_MEM,
  FUNC_EXIT
};

// tags to communicate between emulate() and getContainer()
enum TYPE { MEMID = 1, REGID = 2 };

// map user function with its size
map<unsigned long long, unsigned int> procMap;
map<unsigned long long, char *> procNameMap;

map<unsigned long long, blockInfo *>
    blockMap;  // information regarding a block in structured manner

typedef map<unsigned long long, vector<unsigned long long>> addrVectorMap;

map<unsigned long long, unsigned char> pc_ins_map;

// return structure to communication between emulate and getContainer
struct callInfo {
  TYPE type;
  unsigned long long addr;  // address or register id
};

struct EventIndiCall {
  unsigned long long raddr3;  // furthest caller
  unsigned long long raddr2;
  unsigned long long raddr1;  // nearest caller
  unsigned long long iaddr;   // call point
  unsigned long long taddr;   // call target
};

struct EventIndiCall iCollection[MAX_DATA];
int iCollectionCounter = 0;

bool isBackedup;

ofstream writefd;  // write back to cfg table
ofstream logfd;    // write back to cfg table

addrVectorMap monitorCallerMap;  // map of instruction to reach, function in cfg

bool isExist(EventIndiCall ev);
struct callInfo getContainer(Instruction ins);
void loadBinary(triton::format::elf::Elf *binary);
void symbolizeInputs(seedMap seed);
void initContext(unsigned long long key, bool first);
#endif