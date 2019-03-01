/*
 * Project: Compile time instrumentation Tool CLang
 * Author: Mustakimur Rahman Khandaker (mrk15e@my.fsu.edu)
 * Florida State University
 * Supervised: Dr. Zhi Wang
 */
#include <ctime>
#include <sstream>
#include <string>

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Lex/Lexer.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::driver;
using namespace clang::tooling;
using namespace std;

static llvm::cl::OptionCategory CFILB_CODE_PREP("CFILB CODE PREP TOOL");

/* cfilb_init() instrmentation based on ASTMatcher */
class CFILBWrapMain : public MatchFinder::MatchCallback {
 private:
  bool flagWritten;  // there could be only one main in a source file [C++ need
                     // to be checked]

 public:
  CFILBWrapMain(Rewriter &Rewrite) : Rewrite(Rewrite) {
    srand(time(NULL));  // use random to generate naming for main alias
    flagWritten = false;
  }

  // matched with main method
  // wrap the main method to avoid return to system while there is an indirect
  // call inside the main
  virtual void run(const MatchFinder::MatchResult &Result) {
    int counter = rand() % 1000 + 1;

    SourceManager &sourceManager = Result.Context->getSourceManager();
    const FunctionDecl *func =
        Result.Nodes.getNodeAs<clang::FunctionDecl>("mainFunc");

    // main method with body detected for first time in a source file
    if (func->hasBody() && !flagWritten) {
      // collect function body code
      Stmt *FuncBody = func->getBody();

      string retType = func->getReturnType().getAsString();
      // collect number of params in the function
      unsigned int paramNum = func->getNumParams();

      // we create text for function body and signature
      string funcParamSignature = "";
      // we create text for call the target function from wrap function
      string argString = "";

      for (unsigned int i = 0; i < paramNum; i++) {
        // param_type param_name
        funcParamSignature += func->getParamDecl(i)->getType().getAsString() +
                              " " + func->getParamDecl(i)->getName().str();
        // argument_name
        argString += func->getParamDecl(i)->getName().str();
        if (i < paramNum - 1) {
          funcParamSignature += ", ";
          argString += ", ";
        }
      }


      clang::SourceLocation _b = FuncBody->getBeginLoc(),
                            _e = FuncBody->getEndLoc();
      const char *b = sourceManager.getCharacterData(_b);
      const char *e = sourceManager.getCharacterData(_e);
      string funcString = string(b, e - b + 1);

      // add alias main method and write the collected code inside
      SourceLocation ST_END = func->getBeginLoc().getLocWithOffset(0);
      std::stringstream SSNewMain;
        SSNewMain
            << "\n" + retType +" orig" + std::to_string(counter) +
                   "One_main("+ funcParamSignature +") \n" +
                   funcString + "\n" + retType + " orig" + std::to_string(counter) +
                   "Two_main("+ funcParamSignature +") {\n return orig" +
                   std::to_string(counter) + "One_main("+ argString + ");\n" +
                   "}" + retType + " orig" + std::to_string(counter) +
                   "Three_main("+ funcParamSignature +") {\n return orig" +
                   std::to_string(counter) + "Two_main("+ argString + ");\n" +
                   "}\n";
      
      Rewrite.InsertText(ST_END, SSNewMain.str(), true, true);

      std::stringstream SSReplaceBody;
        SSReplaceBody
            << "{\n  return orig" + std::to_string(counter) +
                   "Three_main("+ argString + ");\n}";

      Rewrite.ReplaceText(FuncBody->getSourceRange(), SSReplaceBody.str());

      flagWritten = true;
    }
  }

 private:
  Rewriter &Rewrite;
};

// AST Consummer class for ccfi first pass clang tool
class CFILBASTConsumer : public ASTConsumer {
 public:
  CFILBASTConsumer(Rewriter &R) : handleCFILB_Init(R) {
    // define the matcher to main method defination
    Matcher.addMatcher(functionDecl(hasName("main")).bind("mainFunc"),
                       &handleCFILB_Init);
  }

  // call the ASTMatch
  void HandleTranslationUnit(ASTContext &Context) override {
    Matcher.matchAST(Context);
  }

 private:
  CFILBWrapMain
      handleCFILB_Init;  // handler to call CFILBWrapMain() in processing
  MatchFinder Matcher;  // ASTMatcher for CCFI_CLANG_FIRST_PASS Tool
};

/* CFILBFrontEndAction - CLang Front-end Action Tool */
class CFILBFrontEndAction : public ASTFrontendAction {
 public:
  CFILBFrontEndAction() {}
  // rewrite done at the end of source file processing
  void EndSourceFileAction() override {
    TheRewriter.getEditBuffer(TheRewriter.getSourceMgr().getMainFileID())
        .write(llvm::outs());
  }

  // initiate rewriter tool and initiate ASTConsumer with it
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 StringRef file) override {
    TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
    return llvm::make_unique<CFILBASTConsumer>(TheRewriter);
  }

 private:
  Rewriter TheRewriter;  // rewriter tool from clang
};

/* ccfi_clang_first_pass main method */
/* compiler intitialization and front-end action tool creation */
int main(int argc, const char **argv) {
  CommonOptionsParser op(argc, argv, CFILB_CODE_PREP);
  ClangTool Tool(op.getCompilations(), op.getSourcePathList());

  /* CFILBFrontEndAction - CLang Front-end Action Tool */
  return Tool.run(newFrontendActionFactory<CFILBFrontEndAction>().get());
}