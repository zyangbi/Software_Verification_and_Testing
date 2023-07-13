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
// #define __DEBUG__

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

    // Get straight line basic blocks using dominator tree
    straightLineBBs.clear();
    DominatorTree &domTree = getAnalysis<DominatorTreeWrapperPass>().getDomTree();
    if (!getStraightLineBBs(F, domTree)) { // result saves to straightLineBBs
      debug << "\n\n ------------getStraightLineBBs Fail-----------------";
      return false;
    }

    // Get decVarSet
    decVarSet.clear();
    for (auto b : topoSortBBs(F)) {
      // Iterate over all the instructions within a basic block, get decVarSet. 
      for (BasicBlock::const_iterator It = b->begin(); It != b->end(); ++It) {
        Instruction *ins = const_cast<llvm::Instruction *>(&*It);
        getDecVarSet(ins);
      }
    }
    printSet(decVarSet);
    
    bool change = true;
    // Chaotic iteration, loop until all entrySet and exitSet don't change
    while (change) {
      change = false;
      for (auto b : topoSortBBs(F)) {
        taintSet.clear();

        // EntrySet of a basic block is the union of all exitSets of the predecessors
        for (auto pred_it = pred_begin(b); pred_it != pred_end(b); ++ pred_it) {
          unordered_set<Value *> predExitSet = exitSetMap[*pred_it];
          taintSet.insert(predExitSet.begin(), predExitSet.end());
        }

        // If entrySet is different from previous entrySet, set flag and update exitSetMap
        if (taintSet != entrySetMap[b]) {
          change = true;
          entrySetMap[b] = taintSet;
        }

        // Iterate over all the instructions within a basic block, update taintSet. 
        for (BasicBlock::const_iterator It = b->begin(); It != b->end(); ++It) {
          debug << "\n";

          Instruction *ins = const_cast<llvm::Instruction *>(&*It);
          checkTainted(ins);

          ins->print(debug);
          debug << "\n";
          printSet(taintSet);        
        }

        // If exitSet is different from previous exitSet, set flag and update exitSetMap
        if (taintSet != exitSetMap[b]) {
          change = true;
          exitSetMap[b] = taintSet;
        }
      }
    }

    // Print final taintSet(exitSet)
    output << "Tainted: ";
    outputTaintSet();

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
  unordered_set<Value *> taintSet;
  unordered_set<Value *> decVarSet;
  unordered_map<BasicBlock *, unordered_set<Value *>> entrySetMap;
  unordered_map<BasicBlock *, unordered_set<Value *>> exitSetMap;
  unordered_set<BasicBlock *> straightLineBBs;

  // Get the set of user decleared variables. These vars should be printed out.
  void getDecVarSet(Instruction *I) {
    if (AllocaInst *allocaInst = dyn_cast<AllocaInst>(I)) {
      if (allocaInst->getName().str() != "retval") {
        debug << "-------------Declared variable " << allocaInst->getName() << " -------------\n";
        decVarSet.insert(allocaInst);
      }
    }
  }

  // Check tainted and untainted variables on each instruction
  void checkTainted(Instruction *I) {

    // Call Instruction
    if (CallInst *callInst = dyn_cast<CallInst>(I)) {
      // 1. Return value of cin is tainted
      if (callInst->getOperand(0)->getName().str().find("cin") != string::npos) {
        Value* input = callInst->getOperand(1);
        debug << "-------------Variable " << input->getName() << " tainted by cin-------------\n";
        printTaintedLine(input, I);
        taintSet.insert(input);
      }

      // 2. Return value of a function call
      else {
        bool tainted = false;
        for (auto argIt = callInst->arg_begin(); argIt != callInst->arg_end(); ++argIt) {
          Value *arg = *argIt;
          if (isInTaintSet(arg)) {
            tainted = true;
            break;
          }
        }

        if (tainted) {
          // Return value is tainted if any argument is tainted
          debug << "-------------Return value " << callInst->getName() << " is tainted by function call " << "--------------\n";
          printTaintedLine(callInst, I);
          taintSet.insert(callInst);
        } else if (isStraightLine(callInst)) {
          // Return value is untainted if all arguments are not tainted and in straigt line code
          debug << "-------------Return value " << callInst->getName() << " is untainted by function call " << "--------------\n";
          printUntaintedLine(callInst, I);
          taintSet.erase(callInst);
        }
      }

    }

    // Store Instruction
    else if (StoreInst *storeInst = dyn_cast<StoreInst>(I)) {
      Value *value = storeInst->getValueOperand();
      Value *pointer = storeInst->getPointerOperand();

      // 3. Assign a var to another var
      if (isInTaintSet(value)) {
        // Variable is tainted if assigned by a tainted var
        debug << "-------------Assigned variable " << pointer->getName() << " tainted by " << value->getName() << "-------------\n";
        printTaintedLine(pointer, I);
        taintSet.insert(pointer);
      } else if (isStraightLine(storeInst)) {
        // Variable is untainted if assigned by an untainted var and in straight line code
        debug << "-------------Assigned variable " << pointer->getName() << " untainted by " << value->getName() << "-------------\n";
        printUntaintedLine(pointer, I);
        taintSet.erase(pointer);
      }
    }

    // Load Instruction
    else if (LoadInst *loadIns = dyn_cast<LoadInst>(I)) {
      Value *pointer = loadIns->getPointerOperand();

      // 4. Load from var to another var
      if (isInTaintSet(pointer)) {
        // Variable is tainted if loaded from a tainted var
        debug << "-------------Loaded variable " << loadIns->getName() << " tainted by " << pointer->getName() << "-------------\n";
        printTaintedLine(loadIns, I);
        taintSet.insert(loadIns);
      } else if (isStraightLine(loadIns)) {
        // Variable is untainted if loaded from an untainted var and in straight line code
        debug << "-------------Loaded variable " << loadIns->getName() << " untainted by " << pointer->getName() << "-------------\n";
        printUntaintedLine(loadIns, I);
        taintSet.erase(loadIns);
      }
    } 

    return;
  } //checkTainted

  bool isInTaintSet(Value *variable) {
    return taintSet.find(variable) != taintSet.end();
  }

  bool hasTaintedArgument(Function *calledFunction) {
    for (auto arg = calledFunction->arg_begin(); arg != calledFunction->arg_end(); ++arg) {
      Value *argument = &(*arg);

      // Check if the argument is a tainted variable
      if (isInTaintSet(argument)) {
        return true;
      }
    }
    return false;
  }

  void outputTaintSet() {
    output << "{";
    bool first = true;
    for (Value *element : taintSet) {
      if (isInDecVarSet(element)) {
        if (!first) {
          output << ",";
        }
        output << element->getName();
        first = false;
      }
    }
    output << "}\n\n";
  }

  void printTaintedLine(Value *var, Instruction *I) {
    if (isInDecVarSet(var) && !isInTaintSet(var)) {
      output << "Line " << getSourceCodeLine(I) << ": " << var->getName() << " is tainted\n";
    }
  }

  void printUntaintedLine(Value *var, Instruction *I) {
    if (isInDecVarSet(var) && isInTaintSet(var)) {
      output << "Line " << getSourceCodeLine(I) << ": " << var->getName() << " is now untainted\n";
    }
  }
  
  bool isInDecVarSet(Value *variable) {
    return decVarSet.find(variable) != decVarSet.end();
  }

  void printSet(unordered_set<Value *> &set) {
    debug << "{";
    bool first = true;
    for (Value *element : set) {
      if (!first) {
        debug << ",";
      }
      debug << element->getName();
      first = false;
    }
    debug << "}\n\n";
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
