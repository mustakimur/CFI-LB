#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include <llvm/Bitcode/BitcodeReader.h> /// for isBitcode
#include <llvm/IRReader/IRReader.h>     /// for isIRFile

#include <fstream>
#include <map>
#include <set>
#include <vector>

using namespace llvm;

static cl::opt<std::string> dirPath("DIR_PATH",
                                    cl::desc("give the program path directory"),
                                    cl::value_desc("directory path"));

typedef std::vector<unsigned long> contextList;
typedef std::vector<unsigned long>::iterator contextListIt;
typedef std::pair<unsigned long, contextList> ctxToTargetPair;
typedef std::set<ctxToTargetPair> ctxToTargetSet;
typedef std::set<ctxToTargetPair>::iterator ctxToTargetSetIt;
typedef std::map<unsigned long, ctxToTargetSet> pointToECMap;
typedef std::map<unsigned long, ctxToTargetSet>::iterator pointToECMapIt;
typedef std::map<unsigned long, int> pointToDepth;
typedef std::map<unsigned long, int>::iterator pointToDepthIt;

class INSTCFG : public ModulePass {
private:
  void replaceGLBUsage(GlobalVariable *New, GlobalVariable *Old) {
    std::set<User *> users;
    for (Value::user_iterator u = Old->user_begin(); u != Old->user_end();
         ++u) {
      User *user = *u;
      users.insert(user);
    }
    for (std::set<User *>::iterator u = users.begin(); u != users.end(); ++u) {
      User *user = *u;
      if (isa<GetElementPtrInst>(user)) {
        GetElementPtrInst *gep = dyn_cast<GetElementPtrInst>(user);
        gep->setSourceElementType(New->getValueType());
        for (unsigned I = 0, E = user->getNumOperands(); I < E; ++I) {
          if (user->getOperand(I) == Old) {
            user->setOperand(I, New);
          }
        }
      }
    }
  }

public:
  static char ID;
  INSTCFG() : ModulePass(ID) {
    unsigned long p, t, s1, s2, s3;
    std::ifstream initcallfd;
    std::string path;
    if (dirPath[0] == '~') {
      dirPath.replace(0, 1, std::getenv("HOME"));
    }

    path = dirPath + "/cfilb_depth0.bin";
    initcallfd.open(path.c_str());
    if (initcallfd.is_open()) {
      while (initcallfd >> p >> t) {
        contextList ctx;
        ctxToTargetPair target;
        target = std::make_pair(t, ctx);
        mapPEC[p].insert(target);
        mapPD[p] = 0;
      }
    }
    initcallfd.close();

    path = dirPath + "/cfilb_depth1.bin";
    initcallfd.open(path.c_str());
    if (initcallfd.is_open()) {
      while (initcallfd >> p >> t >> s1) {
        contextList ctx;
        ctx.push_back(s1);
        ctxToTargetPair target;
        target = std::make_pair(t, ctx);
        mapPEC[p].insert(target);
        mapPD[p] = 1;
      }
    }
    initcallfd.close();

    path = dirPath + "/cfilb_depth2.bin";
    initcallfd.open(path.c_str());
    if (initcallfd.is_open()) {
      while (initcallfd >> p >> t >> s1 >> s2) {
        contextList ctx;
        ctx.push_back(s1);
        ctx.push_back(s2);
        ctxToTargetPair target;
        target = std::make_pair(t, ctx);
        mapPEC[p].insert(target);
        mapPD[p] = 2;
      }
    }
    initcallfd.close();

    path = dirPath + "/cfilb_depth3.bin";
    initcallfd.open(path.c_str());
    if (initcallfd.is_open()) {
      while (initcallfd >> p >> t >> s1 >> s2 >> s3) {
        contextList ctx;
        ctx.push_back(s1);
        ctx.push_back(s2);
        ctx.push_back(s3);
        ctxToTargetPair target;
        target = std::make_pair(t, ctx);
        mapPEC[p].insert(target);
        mapPD[p] = 3;
      }
    }
    initcallfd.close();
  }

  virtual inline void getAnalysisUsage(llvm::AnalysisUsage &au) const {
    // declare your dependencies here.
    /// do not intend to change the IR in this pass,
    au.setPreservesAll();
  }

  bool runOnModule(Module &M) override {
    PointerType *int32PtTy = Type::getInt32PtrTy(M.getContext());
    IntegerType *int32Ty = Type::getInt32Ty(M.getContext());
    std::vector<Constant *> listD0, listD1, listD2, listD3;

    for (pointToECMapIt pIt = mapPEC.begin(); pIt != mapPEC.end(); ++pIt) {
      Constant *cPoint_t = ConstantInt::get(int32Ty, pIt->first, false);
      Constant *cPoint = ConstantFolder().CreateIntToPtr(cPoint_t, int32PtTy);
      for (ctxToTargetSetIt tIt = pIt->second.begin(); tIt != pIt->second.end();
           ++tIt) {
        Constant *cTarget_t = ConstantInt::get(int32Ty, tIt->first, false);
        Constant *cTarget =
            ConstantFolder().CreateIntToPtr(cTarget_t, int32PtTy);
        if (tIt->second.size() == 0) {
          listD0.push_back(cPoint);
          listD0.push_back(cTarget);
        } else if (tIt->second.size() == 1) {
          listD1.push_back(cPoint);
          listD1.push_back(cTarget);
          for (auto cIt = (tIt->second).begin(); cIt != (tIt->second).end();
               ++cIt) {
            Constant *cContext_t = ConstantInt::get(int32Ty, *cIt, false);
            Constant *cContext =
                ConstantFolder().CreateIntToPtr(cContext_t, int32PtTy);
            listD1.push_back(cContext);
          }
        } else if (tIt->second.size() == 2) {
          listD2.push_back(cPoint);
          listD2.push_back(cTarget);
          for (auto cIt = (tIt->second).begin(); cIt != (tIt->second).end();
               ++cIt) {
            Constant *cContext_t = ConstantInt::get(int32Ty, *cIt, false);
            Constant *cContext =
                ConstantFolder().CreateIntToPtr(cContext_t, int32PtTy);
            listD2.push_back(cContext);
          }
        } else if (tIt->second.size() == 3) {
          listD3.push_back(cPoint);
          listD3.push_back(cTarget);
          for (auto cIt = (tIt->second).begin(); cIt != (tIt->second).end();
               ++cIt) {
            Constant *cContext_t = ConstantInt::get(int32Ty, *cIt, false);
            Constant *cContext =
                ConstantFolder().CreateIntToPtr(cContext_t, int32PtTy);
            listD3.push_back(cContext);
          }
        }
      }
    }

    ArrayRef<Constant *> blockArray0(listD0);
    // create the constant type and array
    ArrayType *pArrTy0 = ArrayType::get(int32PtTy, listD0.size());
    Constant *blockItems0 = ConstantArray::get(pArrTy0, blockArray0);

    // Global Variable Declarations
    GlobalVariable *gCFG_old0 = M.getGlobalVariable("CFILB_D0");
    gCFG_old0->setDSOLocal(false);

    GlobalVariable *gvar_cfg_data0 = new GlobalVariable(
        M, blockItems0->getType(), true, GlobalValue::ExternalLinkage,
        blockItems0, "CFILB_D0");

    replaceGLBUsage(gvar_cfg_data0, gCFG_old0);

    Constant *cfgLen0 = ConstantInt::get(int32Ty, listD0.size(), false);
    GlobalVariable *gCFG_len0 = M.getGlobalVariable("CFILB_D0_C");
    gCFG_len0->setInitializer(cfgLen0);

    // CFI-LB Depth 1
    ArrayRef<Constant *> blockArray1(listD1);
    ArrayType *pArrTy1 = ArrayType::get(int32PtTy, listD1.size());
    Constant *blockItems1 = ConstantArray::get(pArrTy1, blockArray1);

    GlobalVariable *gCFG_old1 = M.getGlobalVariable("CFILB_D1");
    gCFG_old1->setDSOLocal(false);

    GlobalVariable *gvar_cfg_data1 = new GlobalVariable(
        M, blockItems1->getType(), true, GlobalValue::ExternalLinkage,
        blockItems1, "CFILB_D1");

    replaceGLBUsage(gvar_cfg_data1, gCFG_old1);

    Constant *cfgLen1 = ConstantInt::get(int32Ty, listD1.size(), false);
    GlobalVariable *gCFG_len1 = M.getGlobalVariable("CFILB_D1_C");
    gCFG_len1->setInitializer(cfgLen1);

    // CFI-LB Depth 2
    ArrayRef<Constant *> blockArray2(listD2);
    ArrayType *pArrTy2 = ArrayType::get(int32PtTy, listD2.size());
    Constant *blockItems2 = ConstantArray::get(pArrTy2, blockArray2);

    GlobalVariable *gCFG_old2 = M.getGlobalVariable("CFILB_D2");
    gCFG_old2->setDSOLocal(false);

    GlobalVariable *gvar_cfg_data2 = new GlobalVariable(
        M, blockItems2->getType(), true, GlobalValue::ExternalLinkage,
        blockItems2, "CFILB_D2");

    replaceGLBUsage(gvar_cfg_data2, gCFG_old2);

    Constant *cfgLen2 = ConstantInt::get(int32Ty, listD2.size(), false);
    GlobalVariable *gCFG_len2 = M.getGlobalVariable("CFILB_D2_C");
    gCFG_len2->setInitializer(cfgLen2);

    // CFI-LB Depth 3
    ArrayRef<Constant *> blockArray3(listD3);
    ArrayType *pArrTy3 = ArrayType::get(int32PtTy, listD3.size());
    Constant *blockItems3 = ConstantArray::get(pArrTy3, blockArray3);

    GlobalVariable *gCFG_old3 = M.getGlobalVariable("CFILB_D3");
    gCFG_old3->setDSOLocal(false);

    GlobalVariable *gvar_cfg_data3 = new GlobalVariable(
        M, blockItems3->getType(), true, GlobalValue::ExternalLinkage,
        blockItems3, "CFILB_D3");

    replaceGLBUsage(gvar_cfg_data3, gCFG_old3);

    Constant *cfgLen3 = ConstantInt::get(int32Ty, listD3.size(), false);
    GlobalVariable *gCFG_len3 = M.getGlobalVariable("CFILB_D3_C");
    gCFG_len3->setInitializer(cfgLen3);

    Function *CFILB_REF_D0 = M.getFunction("cfilb_monitor_d0");
    Function *CFILB_REF_D1 = M.getFunction("cfilb_monitor_d1");
    Function *CFILB_REF_D2 = M.getFunction("cfilb_monitor_d2");
    Function *CFILB_REF_D3 = M.getFunction("cfilb_monitor_d3");

    unsigned long callID;
    for (Function &Fn : M) {
      for (BasicBlock &BB : Fn) {
        for (Instruction &Inst : BB) {
          Instruction *inst = &Inst;
          if (isa<CallBase>(inst)) {
            CallBase *call = dyn_cast<CallBase>(inst);
            if (call->getCalledFunction() &&
                call->getCalledFunction()->getName() ==
                    "cfilb_reference_monitor") {
              Value *idValue = call->getArgOperand(0);
              if (isa<ConstantInt>(idValue)) {
                ConstantInt *cint = dyn_cast<ConstantInt>(idValue);
                callID = cint->getZExtValue();

                if (mapPD.find(callID) != mapPD.end()) {
                  int d = mapPD[callID];
                  if (d == 0) {
                    call->setCalledFunction(CFILB_REF_D0);
                  } else if (d == 1) {
                    call->setCalledFunction(CFILB_REF_D1);
                  } else if (d == 2) {
                    call->setCalledFunction(CFILB_REF_D2);
                  } else if (d == 3) {
                    call->setCalledFunction(CFILB_REF_D3);
                  }
                }
              }
            }
          }
        }
      }
    }

    return true; // must return true if module is modified
  }

private:
  pointToECMap mapPEC;
  pointToDepth mapPD;
};

char INSTCFG::ID = 0;
static RegisterPass<INSTCFG> Trans("llvm-inst-cfg",
                                   "LLVM Instrumentation of dCFG and cCFG");