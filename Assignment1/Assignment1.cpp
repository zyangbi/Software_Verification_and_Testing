// Assignment 1 Template
// Assignment1_template.cpp

#include "llvm/ADT/SCCIterator.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
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

// Demangles the function name.
std::string demangle(const char *name) {
  int status = -1;

  std::unique_ptr<char, void (*)(void *)> res{
      abi::__cxa_demangle(name, NULL, NULL, &status), std::free};
  return (status == 0) ? res.get() : std::string(name);
}

// Function to attach debug Metadata to an instruction
void addDebugMetaData(Instruction *I, char *debugInfo) {
  LLVMContext &C = I->getContext();
  MDNode *N = MDNode::get(C, MDString::get(C, debugInfo));
  char DebugMetadata[100];
  strcpy(DebugMetadata, "cpen400Debug.");
  strcat(DebugMetadata, debugInfo);
  I->setMetadata(DebugMetadata, N);
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

namespace {
struct Assignment1 : public FunctionPass {
  static char ID;

  // Vector to store the line numbers at which undefined
  // variable(s) is(are) used.
  unordered_set<int> BuggyLines;
  
  // EntrySet and ExitSet
  unordered_set<Value *> entrySet;
  unordered_map<BasicBlock *, unordered_set<Value *>> exitSetMap;

  // Keep track of all the functions we have encountered so far.
  unordered_map<string, bool> funcNames;

  // Reset all global variables when a new function is called.
  void cleanGlobalVariables() {

    BuggyLines.clear();
    output_str = "";
    debug_str = "";
  }

  Assignment1() : FunctionPass(ID) {}

  // Complete this function.
  // The function should insert the buggy line numbers
  // in the "BuggyLines" vector.
  void checkUseBeforeDef(Instruction *I, BasicBlock *b) {

    bool isBug = false;

    // Add MetaData to an Alloca instruction.
    // if (isa<llvm::AllocaInst>(I))
    //   addDebugMetaData(I, "This_is_an_alloca_instruction");

    // Add code here...
    // Alloca instruction
    if (AllocaInst *allocIns = dyn_cast<AllocaInst>(I)) {
      entrySet.insert(allocIns);
    } 
    
    // Store Instruction
    else if (StoreInst *storeIns = dyn_cast<StoreInst>(I)) {
      Value *value = storeIns->getValueOperand();
      Value *pointer = storeIns->getPointerOperand();
      // If value is in EntrySet, this is a bug
      if (entrySet.count(value)) {
        entrySet.insert(pointer);
        isBug = true;
      }
      // If value not in EntrySet and pointer in EntrySet, remove pointer from EntrySet
      else if (entrySet.count(pointer)) {
        entrySet.erase(pointer);
      } 
    } 
    
    // Load Instruction
    else if (LoadInst *loadIns = dyn_cast<LoadInst>(I)) {
      Value *pointerOperand = loadIns->getPointerOperand();
      if (entrySet.count(pointerOperand)) {
        isBug = true;
        entrySet.insert(loadIns);
      }
    }

    if (isBug) {
      int line = getSourceCodeLine(I);
      if (line > 0)
        BuggyLines.insert(line);
    }

    return;
  }

  // Function to return the line numbers that uses an undefined variable.
  bool runOnFunction(Function &F) override {

    std::string funcName = demangle(F.getName().str().c_str());

    // Remove all non user-defined functions and functions
    // that starts with '_' or has 'std'.
    if (F.isDeclaration() || funcName[0] == '_' ||
        funcName.find("std") != std::string::npos)
      return false;

    // Remove all functions that we have previously encountered.
    if (funcNames.find(funcName) != funcNames.end())
      return false;

    funcNames.insert(make_pair(funcName, true));

    // Demangle function name and print it.
    // debug << "\n\n---------New Function---------"
    //       << "\n";
    // debug << funcName << "\n";
    // debug << "--------------------------"
    //       << "\n\n";

    // Clear exitSetMap at the start of function
    exitSetMap.clear();
    
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
        checkUseBeforeDef(ins, b);
      }
      
      // Save final entrySet into exitSetMap
      exitSetMap[b] = entrySet;
    }

    // Export data from Set to Vector
    vector<int> temp;
    for (auto line : BuggyLines) {
      temp.push_back(line);
    }

    // Sort vector
    std::sort(temp.begin(), temp.end());

    // // Print the source code line number(s).
    // for (auto line : temp) {
    //   output << funcName << " : " << line << "\n";
    // }

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
};
} // namespace

char Assignment1::ID = 0;
static RegisterPass<Assignment1> X("undeclvar",
                                   "Pass to find undeclared variables");
