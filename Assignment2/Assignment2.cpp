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

  Assignment2() : FunctionPass(ID) {}

  // Add DominatorTree
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<DominatorTreeWrapperPass>();
    AU.setPreservesAll();
  }

  bool runOnFunction(Function &F) override {
    // Only consider "main" function
    string funcName = demangle(F.getName().str().c_str());
    if (funcName != "main") {
      return false;
    }

    // Clear exitSetMap at the start of function
    exitSetMap.clear();

    while (statusChanged) {
      statusChanged = false;
      // Iterate through basic blocks of the function.
      for (auto b : topoSortBBs(F)) {
        // EntrySet of a basic block is the union of all exitSets of the predecessors
        entrySet.clear();
        for (auto pred_it = pred_begin(b); pred_it != pred_end(b); ++ pred_it) {
        unordered_set<Value *> predExitSet = exitSetMap[*pred_it];
        entrySet.insert(predExitSet.begin(), predExitSet.end());
        }
        // Iterate over all the instructions within a basic block.
        for (BasicBlock::const_iterator It = b->begin(); It != b->end(); ++It) {
        Instruction *ins = const_cast<llvm::Instruction *>(&*It);
        checkTainted(ins, b);
        }
        // Save final entrySet into exitSetMap
          exitSetMap[b] = entrySet;
      }
    }

    // Print tainted variables in the final entrySet(exitSetMap[b])
    output << "{";
    for (auto taintedVar : entrySet) {
      output  << taintedVar->getName() << ",";
    }
    output << "}";

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
  }


private:
  unordered_set<Value *> entrySet;
  unordered_map<BasicBlock *, unordered_set<Value *>> exitSetMap;
  unordered_set<BasicBlock *> straightLineBBs;
  bool statusChanged = true;

  // Complete this function.
  void checkTainted(Instruction *I, BasicBlock *b) {

    // 3 possibilies of tainting a variable
    if (CallInst* callInst = dyn_cast<CallInst>(I)) {
      debug << "1\n";
      // 1. find cin assignments
      std::string instructionStr;
          llvm::raw_string_ostream instructionStream(instructionStr);
          I->print(instructionStream);
      debug << instructionStream.str() << "\n";
      if (demangle(I->getOperand(0)->getName().str().c_str()) == "cin") {
        // Get the variable being assigned to, which is tainted at this point
        Value* variable = callInst->getArgOperand(0);
        // Process the assignment from cin to the variable
        statusChanged = true;
        entrySet.insert(variable);
        debug << "A var was just tainted due to cin: " << variable->getName() << "\n";
      }
    } else if (StoreInst* storeInst = dyn_cast<StoreInst>(I)) {
      debug << "in\n";
      Value* value = storeInst->getValueOperand();
          Value* pointer = storeInst->getPointerOperand();
      // Check if the store instruction assigns the return value of a function
          if (auto* callInst = dyn_cast<CallInst>(value)) {
        Function* calledFunction = callInst->getCalledFunction();
        if (calledFunction && isTaintedArgument(calledFunction)) {
          // 2. find tainted vars passed to function
          statusChanged = true;
          entrySet.insert(pointer);
          debug << "A var was just tainted due to tainted function param: " << pointer->getName() << "\n";
        }
      } else if (isInTaintedSet(value)) {
        debug << "5\n";
        // 3. find assignments from a tainted var to another var
        statusChanged = true;
        entrySet.insert(pointer);
        debug << "Tainted variable " << value->getName() << " assigned to " << pointer->getName() << "\n";
      } else {
        debug << "7\n";
        if (isInTaintedSet(pointer)) {
          // untainting a variable
          debug << "8\n";
          entrySet.erase(pointer);
          statusChanged = true;
          debug << "Tainted variable " << pointer->getName() << "is now untainted by assigning " << value->getName() << " to it." << "\n";
        }
      }
    }

    return;
  } //checkTainted

  bool isInTaintedSet(Value *variable) {
    return entrySet.find(variable) != entrySet.end();
  }

  bool isTaintedArgument(Function *calledFunction) {
    for (auto arg = calledFunction->arg_begin(); arg != calledFunction->arg_end(); ++arg) {
      Value *argument = &(*arg);
      // Check if the argument is a tainted variable
      if (isInTaintedSet(argument)) {
        return true;
      }
      }
    return false;
  }


  // Reset all global variables when a new function is called.
  void cleanGlobalVariables() {
    output_str = "";
    debug_str = "";
  }

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

}; // Assignment2
} // namespace

char Assignment2::ID = 0;
static RegisterPass<Assignment2> X("taintanalysis",
                                   "Pass to find tainted variables");
