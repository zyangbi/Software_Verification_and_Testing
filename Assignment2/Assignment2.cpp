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

    // Get straight line basic blocks using dominator tree
    straightLineBBs.clear();
    DominatorTree &domTree = getAnalysis<DominatorTreeWrapperPass>().getDomTree();
    if (!getStraightLineBBs(F, domTree)) { // result saves to straightLineBBs
      debug << "\n\n ------------getStraightLineBBs Fail-----------------";
      return false;
    }
    
    // while (1) {
    // Iterate through basic blocks of the function.
    for (auto b : topoSortBBs(F)) {
      taintSet.clear();

      // EntrySet of a basic block is the union of all exitSets of the predecessors
      for (auto pred_it = pred_begin(b); pred_it != pred_end(b); ++ pred_it) {
        unordered_set<Value *> predExitSet = exitSetMap[*pred_it];
        taintSet.insert(predExitSet.begin(), predExitSet.end());
      }


      // Iterate over all the instructions within a basic block, update taintSet. 
      for (BasicBlock::const_iterator It = b->begin(); It != b->end(); ++It) {
        
        debug << "\n";


        Instruction *ins = const_cast<llvm::Instruction *>(&*It);
        checkTainted(ins, b);


        ins->print(debug);
        debug << "\n";
        printTaintSet();        
      }

      // Save final taintSet into exitSetMap
      exitSetMap[b] = taintSet;
    }
    // }

    // Print tainted variables in the last taintSet
    // printTaintSet();

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
  unordered_map<BasicBlock *, unordered_set<Value *>> entrySetMap;
  unordered_map<BasicBlock *, unordered_set<Value *>> exitSetMap;
  unordered_set<BasicBlock *> straightLineBBs;

  // Complete this function.
  void checkTainted(Instruction *I, BasicBlock *b) {

    // Call Instruction
    if (CallInst* callInst = dyn_cast<CallInst>(I)) {
      // 1. Return value of cin is tainted
      if (callInst->getOperand(0)->getName().str().find("cin") != string::npos) {
        Value* input = callInst->getOperand(1);
        taintSet.insert(input);
        debug << "-------------Variable " << input->getName() << " tainted by cin-------------\n";
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
          taintSet.insert(callInst);
          debug << "-------------Return value " << callInst->getName() << " is tainted by function call " << "--------------\n";
        } else if (isStraightLine(callInst)) {
          // Return value is untainted if all arguments are not tainted and in straigt line code
          taintSet.erase(callInst);
          debug << "-------------Return value " << callInst->getName() << " is untainted by function call " << "--------------\n";
        }
      }

    }

    // Store Instruction
    else if (StoreInst* storeInst = dyn_cast<StoreInst>(I)) {
      Value *value = storeInst->getValueOperand();
      Value *pointer = storeInst->getPointerOperand();

      // 3. Assign a var to another var
      if (isInTaintSet(value)) {
        // Variable is tainted if assigned by a tainted var
        taintSet.insert(pointer);
        debug << "-------------Assigned variable " << pointer->getName() << " tainted by " << value->getName() << "-------------\n";
      } else if (isStraightLine(storeInst)) {
        // Variable is untainted if assigned by an untainted var and in straight line code
        taintSet.erase(pointer);
        debug << "-------------Assigned variable " << pointer->getName() << " untainted by " << value->getName() << "-------------\n";
      }

    }

    // Load Instruction
    else if (LoadInst *loadIns = dyn_cast<LoadInst>(I)) {
      Value *pointer = loadIns->getPointerOperand();

      // 4. Load from var to another var
      if (isInTaintSet(pointer)) {
        // Variable is tainted if loaded from a tainted var
        taintSet.insert(loadIns);
        debug << "-------------Loaded variable " << loadIns->getName() << " tainted by " << pointer->getName() << "-------------\n";
      } else if (isStraightLine(loadIns)) {
        // Variable is untainted if loaded from an untainted var and in straight line code
        taintSet.erase(loadIns);
        debug << "-------------Loaded variable " << loadIns->getName() << " untainted by " << pointer->getName() << "-------------\n";
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

  void printTaintSet() {
    debug << "{";
    for (Value *taint : taintSet) {
      string taintName = taint->getName().str();
      if (!taintName.empty()) {
        debug << taintName << ",";
      }
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
