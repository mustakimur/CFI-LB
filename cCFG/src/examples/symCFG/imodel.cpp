/*
 * Project: Adaptive Control Flow Integrity with Look Back and Multi-scope CFG
 * Generation Author: Mustakimur Rahman Khandaker (mrk15e@my.fsu.edu) Florida
 * State University Supervised: Dr. Zhi Wang
 */
#include "imodel.h"
#include "stack.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctype.h>
#include <map>
#include <math.h>
#include <tuple>
#include <vector>

// maximum number of time same pathconstraints will be tried to solve
#define MAXLOOP 50
// maximum number of instructions will execute in single emulation
#define MAXINST 500

vector<seedMap> waitingSeedList;
vector<seedMap> freshSeedList;

map<triton::uint64, vector<triton::uint512>> usedSeedList;

seedMap initSeed;
EventIndiCall *event;

// test if an entry is already in cfg
bool isExist(EventIndiCall *ev) {
  for (int i = 0; i < iCollectionCounter; i++) {
    if (ev->raddr3 == iCollection[i].raddr3 &&
        ev->raddr2 == iCollection[i].raddr2 &&
        ev->raddr1 == iCollection[i].raddr1 &&
        ev->iaddr == iCollection[i].iaddr &&
        ev->taddr == iCollection[i].taddr) {
      return true;
    }
  }
  return false;
}

void copyToTable(EventIndiCall *ev) {
  iCollection[iCollectionCounter].raddr3 = ev->raddr3;
  iCollection[iCollectionCounter].raddr2 = ev->raddr2;
  iCollection[iCollectionCounter].raddr1 = ev->raddr1;
  iCollection[iCollectionCounter].iaddr = ev->iaddr;
  iCollection[iCollectionCounter].taddr = ev->taddr;
  iCollectionCounter++;

  event->raddr3 = ev->raddr3;
  event->raddr2 = ev->raddr2;
  event->raddr1 = ev->raddr1;
  event->iaddr = ev->iaddr;
  event->taddr = ev->taddr;
}

// write back to cfg
void write_to_callfd(EventIndiCall *ev) {
  string analysisFile = "cfilb_cfg.bin";
  writefd.open(analysisFile.c_str(), std::ofstream::app);
  writefd << ev->iaddr << "\t" << ev->taddr << "\t" << ev->raddr1 << "\t"
          << ev->raddr2 << "\t" << ev->raddr3 << "\n";
  writefd.close();
}

triton::uint64 calMemHash(seedMap seed) {
  triton::uint64 result = 0;
  for (seedMap::iterator item = seed.begin(); item != seed.end(); ++item) {
    result ^= item->first;
  }
  return result;
}

triton::uint512 calDataHash(seedMap seed) {
  triton::uint512 result = 0;
  for (seedMap::iterator item = seed.begin(); item != seed.end(); ++item) {
    result ^= (item->second).data;
  }
  return result;
}

// string to unsigned long long convertion (only convert to less than 8
// bytes)
unsigned long long convertToint(unsigned int size, unsigned char *val) {
  std::stringstream ss;
  std::string::size_type sz = 0;
  unsigned long long data;
  ss << "0x";
  ss << hex;
  for (int i = size - 1; i >= 0; i--) {
    if (int(val[i] < 16)) {
      ss << "0";
    }
    ss << int(val[i]);
  }
  if (size <= 8)
    data = std::stoull(ss.str(), &sz, 0);
  else
    data = 0;
  return data;
}

// instruction stripping to retrieve the direct call target
unsigned long long getTarget(string ins) {
  char *res = (char *)ins.c_str();
  if (res[5] == '0' && res[6] == 'x') {
    for (int i = 7, j = 0; i <= strlen(res); i++, j++) {
      res[j] = res[i];
    }
    return strtoul(res, NULL, 16);
  }
  return 0;
}

// an indirect call target could be in stack or heap memory or in register;
// return the memory address or register id
void getTargetInfo(Instruction ins, callInfo *info) {
  debug("determining call target container ...");

  auto loads = ins.getLoadAccess();
  auto regs = ins.getReadRegisters();

  if (loads.size() > 0) {
    // if target container is a memory address
    info->type = MEMID;
    for (auto load = loads.begin(); load != loads.end(); ++load) {
      info->addr = (load->first).getAddress();
    }
  } else if (regs.size() > 0) {
    // if target container is a register
    info->type = REGID;
    for (auto reg = regs.begin(); reg != regs.end(); ++reg) {
      info->addr = (reg->first).getId();
      if (info->addr != triton::arch::x86::ID_REG_RSP) {
        break;
      }
    }
  }

  if (DEBUG) {
    if (info->type == 1) {
      cout << "... From memory: " << hex << info->addr << dec << endl;
    } else if (info->type == 2) {
      cout << "... From register: " << info->addr << endl;
    } else {
      cout << "... something wrong from getTargetInfo method" << endl;
    }
  }
}

// as typedef seedMap has structure inside, the equality check needs special
// handling
bool isSeedExist(seedMap candidate, seedMap listed) {
  int count = 0;
  for (seedMap::iterator candidateItem = candidate.begin();
       candidateItem != candidate.end(); ++candidateItem) {
    for (seedMap::iterator item = listed.begin(); item != listed.end();
         ++item) {
      if (candidateItem->first == item->first &&
          (candidateItem->second).data == (item->second).data) {
        count++;
      }
    }
  }
  if (count == candidate.size()) {
    return true;
  }
  return false;
}

// seed producer based on discovered path constraints
void getFreshSeeds(seedMap init) {
  int tryCounter = 0;
  int acceptedCounter = 0;

  // get path constraints from the last emulation
  auto pco = api.getPathConstraints();

  debug("# of path constraints discovered: " + to_string(pco.size()));

  // initiate constraint trackers
  auto previousConstraints =
      triton::ast::equal(triton::ast::bvtrue(), triton::ast::bvtrue());

  for (auto pc = pco.begin(); pc != pco.end(); ++pc) {
    // only if a constraint has more than one branches (true and false)
    if (pc->isMultipleBranches()) {
      // retrieve branches for a path constraints
      auto branches = pc->getBranchConstraints();
      for (triton::usize index = 0; index != branches.size(); index++) {
        // get<0> false indicates branch was not taken
        if (std::get<0>(branches[index]) == false) {
          // get<1> provide branch instruction address
          unsigned long long iaddr =
              (unsigned long long)std::get<1>(branches[index]);

          // get<3> is the address if branch is taken; generate a model to let
          // the branch taken
          auto models = api.getModel(triton::ast::assert_(triton::ast::land(
              previousConstraints, std::get<3>(branches[index]))));

          seedMap seed;

          for (auto model = models.begin(); model != models.end(); ++model) {
            // get the required values from symbolic variables
            auto symvar = api.getSymbolicVariableFromId(model->first);
            if (symvar->getKind() != triton::engines::symbolic::REG) {
              seed[symvar->getKindValue()].data = (model->second).getValue();
              seed[symvar->getKindValue()].isSymbolic = true;
              if (init.find(symvar->getKindValue()) != init.end()) {
                seed[symvar->getKindValue()].isWrite =
                    init[symvar->getKindValue()].isWrite;
              } else {
                int look = 1;
                bool flag = false;
                while (look < 20) {
                  if (init.find(symvar->getKindValue() - look) != init.end()) {
                    seed[symvar->getKindValue()].isWrite =
                        init[symvar->getKindValue() - look].isWrite;
                    flag = true;
                    break;
                  }
                  look++;
                }
                if (!flag) {
                  seed[symvar->getKindValue()].isWrite = true;
                }
              }
            }
          }

          if (seed.size() > 0) {
            // initial seeds memories set as concrete value as they are not
            // required to be symbolic anymore as per path constraints
            for (auto item = init.begin(); item != init.end(); ++item) {
              if (seed.find(item->first) == seed.end()) {
                seed[item->first].data = init[item->first].data;
                seed[item->first].isSymbolic = init[item->first].isSymbolic;
                seed[item->first].isWrite = init[item->first].isWrite;
              }
            }
            // if success in having contents to taken the branch; then add
            // it in the vector of input seeds
            bool isMatch = false;

            triton::uint64 key = calMemHash(seed);
            triton::uint512 value = calDataHash(seed);
            if (usedSeedList.find(key) != usedSeedList.end()) {
              if (find(usedSeedList[key].begin(), usedSeedList[key].end(),
                       value) != usedSeedList[key].end()) {
                isMatch = true;
              }
            }

            if (!isMatch) {
              // a real fresh seed pushed back to waitingSeedList
              acceptedCounter++;
              freshSeedList.push_back(seed);
            }
          }

          tryCounter++;

          if (tryCounter > MAX_TRIED_FRESH_SEED ||
              acceptedCounter > MAX_ACCEPTED_FRESH_SEED) {
            api.clearPathConstraints();
            for (vector<seedMap>::iterator fresh = freshSeedList.begin();
                 fresh != freshSeedList.end(); ++fresh) {
              waitingSeedList.push_back(*fresh);
            }
            freshSeedList.clear();
            return;
          }
        }
      }
    }
    // update the constraint tracker with branches is beign taken
    previousConstraints =
        triton::ast::land(previousConstraints, pc->getTakenPathConstraintAst());
  }
  // prepare for next step of emulation based on freashly generated input seeds
  api.clearPathConstraints();
  for (vector<seedMap>::iterator fresh = freshSeedList.begin();
       fresh != freshSeedList.end(); ++fresh) {
    waitingSeedList.push_back(*fresh);
  }
  freshSeedList.clear();
  return;
}

// test if a call target is valid (user function/library function)
bool validTarget(triton::uint512 key) {
  if (procMap.find((unsigned long long)key) == procMap.end() &&
      !isLib((triton::uint64)key)) {
    return false;
  } else {
    return true;
  }
}

// emulate binary (function to start; target reference monitor, seed using)
int emulate(triton::uint64 pc, triton::uint64 exit_addr,
            triton::format::elf::Elf *binary, seedMap seed) {
  int ret_flag = EMULATOR_REGULAR_EXIT;
  unsigned long long start = (unsigned long long)pc;
  blockInfo *blk = blockMap[start];
  unsigned long long mem_addr;
  stack *callStack = new stack();

  bool flagOn = false;
  map<unsigned long long, int> branchExeMap;
  bool isSupported;
  triton::uint512 backupRBP;

  triton::uint64 last_call_ins_addr = 0x0;
  bool hitSuccess = false;
  int max_iteration = 5;

  event = new EventIndiCall;

  while (pc) {
    if (pc == blk->exitAddr) {
      debug("emulator exit the trace header ...");
    }

    // following code process an instruction on address pc
    std::vector<triton::uint8> opcodes = api.getConcreteMemoryAreaValue(pc, 16);

    // sometimes code memory got corrupted, so we verify and fix
    unsigned long long i = 0;
    for (std::vector<triton::uint8>::iterator it = opcodes.begin();
         it != opcodes.end(); ++it, i++) {
      if (*it != pc_ins_map[pc + i]) {
        loadBinary(binary);
        makeRelocation(binary);
        opcodes = api.getConcreteMemoryAreaValue(pc, 16);
        break;
      }
    }

    triton::uint8 *area = nullptr;
    triton::uint32 len = opcodes.size();
    area = new triton::uint8[len];
    for (triton::usize index = 0; index < opcodes.size(); index++)
      area[index] = opcodes[index];
    Instruction instruction;
    instruction.setOpcodes(area, len);
    instruction.setAddress(pc);

    isSupported = api.processing(instruction);

    cout << "[Sym-Emulator] instruction => " << instruction << endl;

    if (!isSupported) {
      if (DEBUG)
        cout << "[Sym-Emulator] instruction is not supported by triton: "
             << instruction << endl;
      pc = pc + instruction.getSize();
      api.setConcreteRegisterValue(Register(triton::arch::x86::ID_REG_RIP, pc));
      continue;
    }

    // if this instruction writes to memory, we need to initialize the memory
    // now
    if (blk->ins_mem_map.find(instruction.getAddress()) !=
        blk->ins_mem_map.end()) {
      mem_addr = blk->ins_mem_map[instruction.getAddress()];
      if (seed[mem_addr].isWrite) {
        if (seed[mem_addr].isSymbolic) {
          api.concretizeAllRegister();
          api.concretizeAllMemory();
          int t = 0;
          while (seed[mem_addr + t].isSymbolic && seed[mem_addr + t].isWrite) {
            api.convertMemoryToSymbolicVariable(
                MemoryAccess(mem_addr + t, 8, seed[mem_addr + t].data));
            api.convertMemoryToSymbolicVariable(
                MemoryAccess(mem_addr + t + 1, 8));
            t++;
          }
        } else {
          int t = 0;
          while (!seed[mem_addr + t].isSymbolic && seed[mem_addr + t].isWrite) {
            api.setConcreteMemoryValue(
                MemoryAccess(mem_addr + t, 8, seed[mem_addr + t].data));
            t++;
          }
        }
      }
    }

    if (DEBUG && instruction.getType() == triton::arch::x86::ID_INS_CALL) {
      cout << "[Sym-Emulator] branch instruction => " << instruction << endl;
    }

    if (pc == exit_addr) {
      // debug("emulator hits the target reference monitor");

      unsigned long long call_target;
      unsigned long long call_point;
      unsigned long long call_point_id;

      // time to look for the call point
      call_point_id = (triton::uint64)api.getConcreteRegisterValue(
        Register(triton::arch::x86::ID_REG_EDI));
      pc = instruction.getNextAddress();
      while (1) {
        std::vector<triton::uint8> t_opcodes =
            api.getConcreteMemoryAreaValue(pc, 16);
        triton::uint8 *t_area = nullptr;
        triton::uint32 t_len = t_opcodes.size();
        t_area = new triton::uint8[t_len];
        for (triton::usize index = 0; index < t_opcodes.size(); index++)
          t_area[index] = t_opcodes[index];
        Instruction t_instruction;
        t_instruction.setOpcodes(t_area, t_len);
        t_instruction.setAddress(pc);
        api.processing(t_instruction);

        if (t_instruction.getType() == triton::arch::x86::ID_INS_CALL) {
          instruction = t_instruction;
          break;
        }
        pc = t_instruction.getNextAddress();
      }

      if (DEBUG) {
        cout << "[Sym-Emulator] target call point reached ==> " << instruction
             << endl;
      }

      call_point = (unsigned long long)instruction.getAddress();
      callInfo *info = new callInfo;
      getTargetInfo(instruction, info);

      if (info->type == MEMID) {
        std::stringstream ss;
        std::string::size_type sz = 0;
        vector<triton::uint8> target_mem;
        // read the target address from memory
        target_mem = api.getConcreteMemoryAreaValue(info->addr, 8);
        ss << "0x";
        ss << hex;
        for (int i = 7; i >= 0; i--) {
          if (int(target_mem[i] < 16)) {
            ss << "0";
          }
          ss << int(target_mem[i]);
        }
        call_target = std::stoull(ss.str(), &sz, 0);
      } else if (info->type == REGID) {
        // read the target address from register
        call_target = (unsigned long long)api.getConcreteRegisterValue(
            Register(info->addr));
      }

      delete info;

      if (!callStack->isStatusValid()) {
        debug("emulator found an illegal call stack status");
        break;
      } else {
        EventIndiCall *ev = new EventIndiCall;
        ev->raddr1 = callStack->head1();
        ev->raddr2 = callStack->head2();
        ev->raddr3 = callStack->head3();
        ev->iaddr = call_point_id;
        ev->taddr = call_target;

        if (hitSuccess && event->raddr1 != ev->raddr1 &&
            !validTarget(ev->taddr)) {
          ev->taddr = event->taddr;
        }

        if (!isExist(ev) && validTarget(ev->taddr)) {
          hitSuccess = true;
          copyToTable(ev);
          cout << hex << ev->iaddr << "\t" << ev->taddr << "\t" << ev->raddr1
               << "\t" << ev->raddr2 << "\t" << ev->raddr3 << dec << endl;
          cout << "==> Hurray!!! look we have discovered another ..." << endl;
          write_to_callfd(ev);
        }
        delete ev;
      }
      pc = callStack->pop();
      api.setConcreteRegisterValue(Register(triton::arch::x86::ID_REG_RIP, pc));
      continue;
    }

    // program emulate to wrong way; we should exit from here
    if (instruction.getType() == triton::arch::x86::ID_INS_HLT) {
      // according to triton engine, we should not proceed further
      debug("emulator exit because HLT instruction");
      break;
    }

    // following part of code is to implement the heuristic approach from
    // static caller graph
    if (instruction.getType() == triton::arch::x86::ID_INS_CALL) {
      last_call_ins_addr = instruction.getAddress();
      // determining call target to; either direct or indirect
      unsigned long long target = getTarget(instruction.getDisassembly());
      // in case indirect call, we try to determine the target, if failed, we
      // move forward to next instruction
      if (target == 0) {
        callInfo *icall = new callInfo;
        getTargetInfo(instruction, icall);

        if (icall->type == MEMID) {
          std::stringstream ss;
          std::string::size_type sz = 0;
          vector<triton::uint8> target_mem;
          // read the target address from memory
          target_mem = api.getConcreteMemoryAreaValue(icall->addr, 8);
          ss << "0x";
          ss << hex;
          for (int i = 7; i >= 0; i--) {
            if (int(target_mem[i] < 16)) {
              ss << "0";
            }
            ss << int(target_mem[i]);
          }
          target = std::stoull(ss.str(), &sz, 0);
        } else if (icall->type == REGID) {
          // read the target address from register
          target = (unsigned long long)api.getConcreteRegisterValue(
              Register(icall->addr));
        }

        delete icall;

        if (!validTarget(target)) {
          pc = instruction.getNextAddress();
          api.setConcreteRegisterValue(
              Register(triton::arch::x86::ID_REG_RIP, pc));
          continue;
        }
      }

      // 1st constraint: if target is a function that is on the cfg for the
      // call point 2nd constraint: if target is a function called a library
      // function otherwise, for now not even emulate it
      if (find(monitorCallerMap[exit_addr].begin(),
               monitorCallerMap[exit_addr].end(),
               target) != monitorCallerMap[exit_addr].end()) {
        callStack->push(instruction.getNextAddress());
        backupRBP = api.getConcreteRegisterValue(
            Register(triton::arch::x86::ID_REG_RBP));
        debug("let's allowed it [static cfg]");
        if (DEBUG)
          cout << "\t\t\t =====>" << procNameMap[target] << endl;
      } else if (procMap.find(target) != procMap.end()) {
        if (!hitSuccess && !flagOn) {
          callStack->push(instruction.getNextAddress());
          flagOn = true;
          debug("single step allowance");
          if (DEBUG)
            cout << "\t\t\t == == = > " << procNameMap[target] << endl;
        } else {
          pc = instruction.getNextAddress();
          debug("let's move forward");
          api.setConcreteRegisterValue(
              Register(triton::arch::x86::ID_REG_RIP, pc));
        }
      }
    } else if (instruction.getType() == triton::arch::x86::ID_INS_RET) {
      if (DEBUG)
        cout << "return to <=====" << endl;
      if (flagOn) {
        flagOn = false;
      }
      if (callStack->isEmpty()) {
        debug("Error: return insruction got an empty stack");
        break;
      } else {
        pc = callStack->pop();
        api.setConcreteRegisterValue(
            Register(triton::arch::x86::ID_REG_RIP, pc));
      }
    } else if (instruction.isControlFlow()) {
      // no need to count it for call stack
      int ret = hookingHandler(instruction.getAddress(), last_call_ins_addr,
                               seed, blk);
      if (ret == LIBC_MATCHED) {
        // everything is fine here
        debug("let's allowed it [regular-library]");
      } else if (ret == DISCOVERED_EXTERNAL_LIBC) {
        ret_flag = DISCOVERED_EXTERNAL_LIBC;
        debug("let's allowed it [re-initiate]");
        // break;
      } else if (ret == UNEXPECTED_TERMINATOR) {
        if (!flagOn) {
          pc = instruction.getNextAddress();
          api.setConcreteRegisterValue(
              Register(triton::arch::x86::ID_REG_RIP, pc));
          debug("exit touch when flagOn is false");
          continue;
        } else {
          flagOn = false;
          if (callStack->isEmpty()) {
            debug("exit touch when flagOn is true but call stack is empty");
            break;
          } else {
            pc = callStack->pop();
            api.setConcreteRegisterValue(
                Register(triton::arch::x86::ID_REG_RIP, pc));
            api.setConcreteRegisterValue(
                Register(triton::arch::x86::ID_REG_RBP, backupRBP));
            debug("exit touch when flagOn is false but call stack is fine");
            continue;
          }
        }
      } else if (ret == NO_LIBC_MATCHED) {
        // another heuristic approach to control the loop iteration

        string ins_str = instruction.getDisassembly();

        if (ins_str.find("jmp") != string::npos &&
            ins_str.find("jmp 0x") == string::npos) {
          triton::uint64 tpc = (triton::uint64)api.getConcreteRegisterValue(
              Register(triton::arch::x86::ID_REG_RIP));
          for (map<unsigned long long, unsigned int>::iterator it =
                   procMap.begin();
               it != procMap.end(); ++it) {
            if (pc >= it->first && pc <= (it->first + it->second)) {
              if (tpc >= it->first && tpc <= (it->first + it->second)) {
              } else {
                pc = instruction.getNextAddress();
                api.setConcreteRegisterValue(
                    Register(triton::arch::x86::ID_REG_RIP, pc));
                continue;
              }
            }
          }
        }

        if (branchExeMap.find(instruction.getAddress()) == branchExeMap.end()) {
          branchExeMap[instruction.getAddress()] = 1;
        } else {
          branchExeMap[instruction.getAddress()]++;
        }
        // we use a threshold of 3
        if (branchExeMap[instruction.getAddress()] > max_iteration) {
          if (DEBUG)
            cout << "max iteration reached, so move to next iteration ... "
                 << endl;
          pc = instruction.getNextAddress();
          api.setConcreteRegisterValue(
              Register(triton::arch::x86::ID_REG_RIP, pc));
          continue;
        }
      }
    }

    // set the emulate instruction address to the next from rip register
    pc = (triton::uint64)api.getConcreteRegisterValue(
        Register(triton::arch::x86::ID_REG_RIP));
  }
  delete callStack;
  delete event;
  return ret_flag;
}

// initialize symbolic but read-first memories
void initSymbolicMemory(seedMap seed) {
  api.concretizeAllRegister();
  api.concretizeAllMemory();
  for (auto mem = seed.begin(); mem != seed.end(); ++mem) {
    if (mem->second.isSymbolic == true && mem->second.isWrite == false) {
      api.convertMemoryToSymbolicVariable(
          MemoryAccess(mem->first, 8, mem->second.data));
      api.convertMemoryToSymbolicVariable(MemoryAccess(mem->first + 1, 8));
    }
  }
  debug("initialize symbolic but read-first memories");
  return;
}

// initialize concrete and read-first memories
void initConcreteMemory(seedMap seed) {
  for (auto mem = seed.begin(); mem != seed.end(); ++mem) {
    if (mem->second.isWrite == false && mem->second.isSymbolic == false) {
      // set concrete value to memory (like memory point to another memory)
      api.setConcreteMemoryValue(MemoryAccess(mem->first, 8, mem->second.data));
    }
  }
  debug("initialize concrete and read-first memories");
}

// set rbp rsp register
void initContext(unsigned long long key) {
  blockInfo *blk = blockMap[key];
  unsigned int size;
  std::vector<triton::uint8> temp;

  // first instruction (once in an emulation) push rbp will fix it
  api.setConcreteRegisterValue(
      Register(triton::arch::x86::ID_REG_RSP, blk->rbp_address + 0x8));
  api.setConcreteRegisterValue(
      Register(triton::arch::x86::ID_REG_RBP, blk->rbp_address + 0x8));

  debug("set rbp rsp for function");
}

// load function block dump to initial seedMap
void initSeeds(unsigned long long key) {
  debug("loading function block dump to initial seedMap");
  initSeed.clear();
  if (blockMap.find(key) == blockMap.end()) {
    if (DEBUG) {
      cout << "Function " << hex << key << dec
           << " is not found inside function block list" << endl;
    }
  }
  // key is the function entry address
  blockInfo *blk = blockMap[key];

  unsigned int size;
  map<unsigned long long, unsigned int>::iterator mem;
  for (mem = blk->mem_size_map.begin(); mem != blk->mem_size_map.end(); ++mem) {
    size = mem->second;
    if (size >= 100)
      continue;
    for (int i = 0; i < size; i++) {
      initSeed[mem->first + i].data = blk->mem_data_map[mem->first][i];
      initSeed[mem->first + i].isSymbolic = blk->mem_sym_map[mem->first];
      initSeed[mem->first + i].isWrite = blk->mem_write_map[mem->first];
    }
  }
}

// retrieve binary raw bytes and do other configurations
void loadBinary(triton::format::elf::Elf *binary) {
  debug("loading target binary");
  // read raw bytes from target binary
  const triton::uint8 *rawData = binary->getRaw();
  // collect information of program header table
  const std::vector<triton::format::elf::ElfProgramHeader> &phddrs =
      binary->getProgramHeaders();

  // iterate over program header table
  for (auto phddr = phddrs.begin(); phddr != phddrs.end(); ++phddr) {
    triton::uint64 offset = phddr->getOffset();
    triton::uint64 size = phddr->getFilesz();
    triton::uint64 vaddr = phddr->getVaddr();

    // read the bytes for each entry
    const std::vector<triton::uint8> temp(rawData + offset,
                                          rawData + offset + size);
    api.setConcreteMemoryAreaValue(vaddr, temp);
    if (!isBackedup) {
      for (unsigned long long i = 0; i < size; i++) {
        unsigned long long addr = vaddr + i;
        pc_ins_map[addr] = rawData[offset + i];
      }
    }
  }

  isBackedup = true;
  return;
}

// read from binary seed dump file and load information in memory
void collectSeedDump() {
  debug("loading seed dump data");
  FILE *seedfd;
  string path = "funcSeedRepo.bin";
  seedfd = fopen(path.c_str(), "rb");
  char tag;
  bool isSymbolic;
  unsigned long long faddr, maddr, eaddr, iaddr;
  unsigned int size;
  unsigned char *val;
  size_t ret;
  if (seedfd != NULL) {
    while (1) {
      ret = fread(&tag, sizeof(char), 1, seedfd);
      if (feof(seedfd)) {
        break;
      }
      if (tag == FUNC_ENTRY) {
        ret = fread(&faddr, SULONG, 1, seedfd);
        blockMap[faddr] = new blockInfo();
      } else if (tag == FUNC_EXIT) {
        ret = fread(&eaddr, SULONG, 1, seedfd);
        blockMap[faddr]->exitAddr = eaddr;
      } else if (tag == REG_RSP_RBP_MOV) {
        ret = fread(&size, SUDEC, 1, seedfd);
        val = (unsigned char *)malloc(size * SUCHAR);
        ret = fread(val, SUCHAR, size, seedfd);
        // rsp for the block
        unsigned long long data = convertToint(size, val);
        blockMap[faddr]->rbp_address = data;
      } else if (tag == REG_MEM_READ) {
        ret = fread(&iaddr, SULONG, 1, seedfd);
        ret = fread(&maddr, SULONG, 1, seedfd);
        ret = fread(&size, SUDEC, 1, seedfd);
        ret = fread(&isSymbolic, SBOOL, 1, seedfd);
        val = (unsigned char *)malloc(size * SUCHAR);
        ret = fread(val, SUCHAR, size, seedfd);

        if (blockMap[faddr]->mem_write_map.find(maddr) ==
            blockMap[faddr]->mem_write_map.end()) {
          // if not already have it once
          blockMap[faddr]->mem_write_map[maddr] = false;
          blockMap[faddr]->mem_size_map[maddr] = size;
          blockMap[faddr]->mem_data_map[maddr] =
              (unsigned char *)malloc(size * SUCHAR);
          memcpy(blockMap[faddr]->mem_data_map[maddr], val, size);
          blockMap[faddr]->mem_sym_map[maddr] = isSymbolic;
        }
      } else if (tag == REG_MEM_WRITE) {
        ret = fread(&iaddr, SULONG, 1, seedfd);
        ret = fread(&maddr, SULONG, 1, seedfd);
        ret = fread(&size, SUDEC, 1, seedfd);
        ret = fread(&isSymbolic, SBOOL, 1, seedfd);
        val = (unsigned char *)malloc(size * SUCHAR);
        ret = fread(val, SUCHAR, size, seedfd);

        if (blockMap[faddr]->mem_write_map.find(maddr) ==
            blockMap[faddr]->mem_write_map.end()) {
          // if already have not it once
          blockMap[faddr]->ins_mem_map[iaddr] = maddr;
          blockMap[faddr]->mem_write_map[maddr] = true;
          blockMap[faddr]->mem_size_map[maddr] = size;
          blockMap[faddr]->mem_data_map[maddr] =
              (unsigned char *)malloc(size * SUCHAR);
          memcpy(blockMap[faddr]->mem_data_map[maddr], val, size);
          blockMap[faddr]->mem_sym_map[maddr] = isSymbolic;
        }
      } else if (tag == GLB_MEM) {
        ret = fread(&iaddr, SULONG, 1, seedfd);
        ret = fread(&maddr, SULONG, 1, seedfd);
        ret = fread(&size, SUDEC, 1, seedfd);
        ret = fread(&isSymbolic, SBOOL, 1, seedfd);
        val = (unsigned char *)malloc(size * SUCHAR);
        ret = fread(val, SUCHAR, size, seedfd);

        if (blockMap[faddr]->mem_write_map.find(maddr) ==
            blockMap[faddr]->mem_write_map.end()) {
          // if already have not it once
          blockMap[faddr]->mem_write_map[maddr] = false;
          blockMap[faddr]->mem_size_map[maddr] = size;
          blockMap[faddr]->mem_data_map[maddr] =
              (unsigned char *)malloc(size * SUCHAR);
          memcpy(blockMap[faddr]->mem_data_map[maddr], val, size);
          blockMap[faddr]->mem_sym_map[maddr] = isSymbolic;
        }
      }
      if (!val)
        free(val);
    }
  }
  fclose(seedfd);
}

// symbolic code coverage emulator
int main(int ac, const char *av[]) {
  if (ac < 4) {
    cout << "usage: ./symEmulator binary_path binary call_point "
            "emulation_start_point"
         << endl;
    return 0;
  }

  string binary_name = string(av[1]); // binary name
  unsigned long long target_call_point = strtoul(av[2], NULL, 10);
  unsigned long long emulation_start_point = strtoul(av[3], NULL, 10);

  // load existing cfg table
  string analysisFile = "cfilb_cfg.bin";
  ifstream initcallfd;
  initcallfd.open(analysisFile.c_str());
  if (initcallfd.is_open()) {
    while (initcallfd >> iCollection[iCollectionCounter].iaddr >>
           iCollection[iCollectionCounter].taddr >>
           iCollection[iCollectionCounter].raddr1 >>
           iCollection[iCollectionCounter].raddr2 >>
           iCollection[iCollectionCounter].raddr3) {
      iCollectionCounter++;
    }
  }
  initcallfd.close();

  // load user function in target binary
  string procFile = "elf_extract.bin";
  ifstream pFile(procFile);
  char name[100];
  unsigned long long start;
  unsigned int size;
  while (pFile >> name >> start >> size) {
    procMap[start] = size;
    procNameMap[start] = (char *)malloc(strlen(name) + 1);
    strcpy(procNameMap[start], name);
  }
  pFile.close();

  int m_addr, f_addr, tag;
  string sym_helper_path = "sym_helper.bin";
  ifstream symHFile(sym_helper_path.c_str());

  while (symHFile >> m_addr >> f_addr) {
    monitorCallerMap[m_addr].push_back(f_addr);
  }
  symHFile.close();

  collectSeedDump();
  debug("loaded pre-computed data for symbolic code coverage");

  // system architecture set as intel x86-64
  api.setArchitecture(ARCH_X86_64);
  // set symbolic optimization mode
  api.enableMode(ALIGNED_MEMORY, true);
  Elf *binary = new Elf(binary_name);

  int ret_emulator;
  bool initAgain = true;
  isBackedup = false;

  loadBinary(binary);
  makeRelocation(binary);
  debug("prepared symbolic code coverage initialization");

  triton::uint64 ENTRY =
      emulation_start_point; // function entry where emulator should start
  triton::uint64 EXIT =
      target_call_point; // instruction of target reference monitor

  // if (DEBUG)
  cout << "[Sym-Emulator] symbolic emulator starts at " << hex << ENTRY << dec
       << " to reach at " << hex << EXIT << dec << endl;

  waitingSeedList.clear();
  freshSeedList.clear();
  usedSeedList.clear();
  unsigned int max_used = 0;

  initSeeds(ENTRY);
  if (initSeed.size() > 0) {
    while (initAgain) {
      initAgain = false;

      waitingSeedList.push_back(
          initSeed); // initial seed pushed to waiting seed list

      while (waitingSeedList.size() > 0) {
        if (DEBUG) {
          cout << "Used seed list size " << max_used << endl;
        }

        seedMap seed = waitingSeedList[0]; // top of the waitingSeedList

        initContext(ENTRY);
        initConcreteMemory(seed);
        initSymbolicMemory(seed);
        simglibcinit();
        debug("initialized emulator memory with scratch seed");

        debug("let's play the emulator ...");

        ret_emulator = emulate(ENTRY, EXIT, binary, seed);

        debug("... emulator have done its task");
        if (ret_emulator == DISCOVERED_EXTERNAL_LIBC) {
          initAgain = true;
          debug("We have to reinitiate everything");
        }

        // processed seed enqued in usedSeedList
        usedSeedList[calMemHash(seed)].push_back(calDataHash(seed));
        max_used++;

        if (max_used > MAX_USED_SEED) {
          initAgain = false;
          break;
        }

        // remove the processed seed from waitingSeedList; its the top of
        // the waitingSeedList
        waitingSeedList.erase(waitingSeedList.begin());

        // get fresh seeds by SMT solver
        if (waitingSeedList.size() < MAX_WAITING_SEED) {
          getFreshSeeds(initSeed);
        }
        if (DEBUG)
          cout << "Waiting seed list size " << waitingSeedList.size() << endl;

        if (initAgain) {
          initSeeds(ENTRY);
          break;
        }
      }
    }
  }
  delete binary;
  debug("symbolic code coverage have done his job");

  return 1;
}