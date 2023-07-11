#include "llvm/ADT/SCCIterator.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Dominators.h"
#include <cxxabi.h>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

using namespace llvm;
using namespace std;

// Comment out the line below to turn off all debug statements.
// **Note** For final submission of this assignment,
//          please comment out the line below.
#define __DEBUG__

// Output strings for debugging
std::string debug_str;
raw_string_ostream debug(debug_str);

// Strings for output
std::string output_str;
raw_string_ostream output(output_str);


namespace {
class Assignment2 : public FunctionPass {
  public:
    static char ID;


    // Reset all global variables when a new function is called.
    void cleanGlobalVariables() {
      output_str = "";
      debug_str = "";
    }

    Assignment2() : FunctionPass(ID) {}

    // Add DominatorTree
    void getAnalysisUsage(AnalysisUsage &AU) const override {
      AU.addRequired<DominatorTreeWrapperPass>();
      AU.setPreservesAll();
    }

    // Function to return tainted variables
    bool runOnFunction(Function &F) override {
      // Only consider "main" function
      string funcName = demangle(F.getName().str().c_str());
      if (funcName != "main") {
        return false;
      }
      // debug << "\n\n---------New Function---------"
      //       << "\n";
      // debug << funcName << "\n";
      // debug << "--------------------------"
      //       << "\n\n";
      

      //---------------------------------------------------------
      // My code
      //---------------------------------------------------------


      straightLineBBs.clear();
      DominatorTree &domTree = getAnalysis<DominatorTreeWrapperPass>().getDomTree();

      if (!getStraightLineBBs(F, domTree)) { // result saves to straightLineBBs
        debug << "\n\n ------------getStraightLineBBs Fail-----------------";
        return false;
      }

      for (BasicBlock *BB : topoSortBBs(F)) {
        for (Instruction &I : *BB) {
          if (isStraightLine(&I)) {
            debug << getSourceCodeLine(&I) << "\n";
          }           
        }
      }

      for (auto bb: straightLineBBs) {
        // debug << bb->getName() << "\n";
      }
      


      //----------------------------------------------------------
      // End
      //----------------------------------------------------------

      // Print debug string if __DEBUG__ is enabled.
      #ifdef __DEBUG__
      errs() << debug.str();
      #endif
      debug.flush();

      // Print output
      errs() << output.str();
      output.flush();

      cleanGlobalVariables();
      return false;
    } // runOnFunction

private:    
  unordered_set<BasicBlock *> straightLineBBs;

  // Demangles the function name.
  std::string demangle(const char *name) {
    int status = -1;

    std::unique_ptr<char, void (*)(void *)> res{
        abi::__cxa_demangle(name, NULL, NULL, &status), std::free};
    return (status == 0) ? res.get() : std::string(name);
  }

  // Returns the source code line number cooresponding to the LLVM instruction.
  // Returns -1 if the instruction has no associated Metadata.
  int getSourceCodeLine(Instruction *I) {
    // Get debugInfo associated with every instruction.
    llvm::DebugLoc debugInfo = I->getDebugLoc();

    int line = -1;
    if (debugInfo)
      line = debugInfo.getLine();

    return line;
  }

  // Topologically sort all the basic blocks in a function.
  // Handle cycles in the directed graph using Tarjan's algorithm
  // of Strongly Connected Components (SCCs).
  vector<BasicBlock *> topoSortBBs(Function &F) {
    vector<BasicBlock *> tempBB;
    for (scc_iterator<Function *> I = scc_begin(&F), IE = scc_end(&F); I != IE;
        ++I) {

      // Obtain the vector of BBs in this SCC and print it out.
      const std::vector<BasicBlock *> &SCCBBs = *I;

      for (std::vector<BasicBlock *>::const_iterator BBI = SCCBBs.begin(),
                                                    BBIE = SCCBBs.end();
          BBI != BBIE; ++BBI) {

        BasicBlock *b = const_cast<llvm::BasicBlock *>(*BBI);
        tempBB.push_back(b);
      }
    }

    reverse(tempBB.begin(), tempBB.end());
    return tempBB;
  }

  // Get all BBs with straight lines
  bool getStraightLineBBs(Function &F, DominatorTree &domTree) {
    BasicBlock &entryBlock = F.getEntryBlock();
    BasicBlock &exitBlock = F.back();

    vector<BasicBlock *> path;
    if (backtraceDomTree(&entryBlock, domTree, &exitBlock, path)) {
      straightLineBBs.insert(path.begin(), path.end());
      return true;
    }
    return false;
  }

  // Backtrace domTree to get the path from entryBlock to exitBlock, which contains all straight line blocks.
  bool backtraceDomTree(BasicBlock *block, DominatorTree &domTree, BasicBlock *exitBlock, vector<BasicBlock *> &path) {
    if (!block) {
      return false;
    }

    if (block == exitBlock) {
      path.push_back(block);
      return true;
    }
    
    DomTreeNode *node = domTree.getNode(block);
    if (node && !node->children().empty()) {
      for (DomTreeNode *childNode : node->children()) {
        path.push_back(node->getBlock());
        if (backtraceDomTree(childNode->getBlock(), domTree, exitBlock, path)) {
          return true;
        }
        path.pop_back();
      }
    }
    return false;
  }

  // Check if an instruction is straight line
  bool isStraightLine(Instruction *I) {
    BasicBlock* block = I->getParent();
    return straightLineBBs.count(block);
  }

}; // Assignment2 (FunctionPass)
} // namespace

char Assignment2::ID = 0;
static RegisterPass<Assignment2> X("taintanalysis",
                                   "Pass to find tainted variables");
