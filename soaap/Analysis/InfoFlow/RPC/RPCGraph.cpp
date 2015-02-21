#include "Analysis/InfoFlow/RPC/RPCGraph.h"
#include "Common/Debug.h"
#include "Util/SandboxUtils.h"

#include "llvm/IR/Constants.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Support/raw_ostream.h"

using namespace soaap;

void RPCGraph::build(SandboxVector& sandboxes, FunctionSet& privilegedMethods, Module& M) {
  /*
   * Find sends
   */
  map<Sandbox*,SmallVector<CallInst*,16>> senderToCalls;

  // privileged methods
  for (Function* F : privilegedMethods) {
    for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
      if (CallInst* C = dyn_cast<CallInst>(&*I)) {
        if (Function* callee = C->getCalledFunction()) {
          if (callee->getName().startswith("__soaap_rpc_send_helper")) {
            SDEBUG("soaap.analysis.infoflow.rpc", 3, dbgs() << "Send in <privileged>: " << *C << "\n");
            senderToCalls[NULL].push_back(C);
          }
        }
      }
    }
  }
  // sandboxed functions
  for (Sandbox* S : sandboxes) {
    for (CallInst* C : S->getCalls()) { 
      if (Function* callee = C->getCalledFunction()) {
        if (callee->getName().startswith("__soaap_rpc_send_helper")) {
          SDEBUG("soaap.analysis.infoflow.rpc", 3, dbgs() << "Send in sandbox " << S->getName() << ": " << *C << "\n");
          senderToCalls[S].push_back(C);
        }
      }
    }
  }

  /*
   * Find receives
   */
  // privileged methods
  map<Sandbox*,map<string,Function*>> receiverToMsgTypeHandler;
  for (Function* F : privilegedMethods) {
    for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
      if (CallInst* C = dyn_cast<CallInst>(&*I)) {
        if (Function* callee = C->getCalledFunction()) {
          if (callee->getName().startswith("__soaap_rpc_recv_helper") 
             || callee->getName().startswith("__soaap_rpc_recv_sync_helper")) {
            SDEBUG("soaap.analysis.infoflow.rpc", 3, dbgs() << "Receive in <privileged>: " << *C << "\n");
            // extract args
            string msgType = "";
            Function* msgHandler = NULL;
            if (GlobalVariable* msgTypeStrGlobal = dyn_cast<GlobalVariable>(C->getArgOperand(1)->stripPointerCasts())) {
              if (ConstantDataArray* msgTypeStrArray = dyn_cast<ConstantDataArray>(msgTypeStrGlobal->getInitializer())) {
                msgType = msgTypeStrArray->getAsCString();
              }
            }
            if (callee->getName().startswith("__soaap_rpc_recv_helper")) {
              msgHandler = dyn_cast<Function>(C->getArgOperand(2)->stripPointerCasts());
            }
            else {
              msgHandler = F;
            }
            receiverToMsgTypeHandler[NULL][msgType] = msgHandler;
            SDEBUG("soaap.analysis.infoflow.rpc", 3, dbgs() << "msg type: " << msgType << "\n");
            SDEBUG("soaap.analysis.infoflow.rpc", 3, dbgs() << "msg handler: " << msgHandler->getName() << "\n");
          }
        }
      }
    }
  }
  // sandboxed functions
  for (Sandbox* S : sandboxes) {
    for (CallInst* C : S->getCalls()) {
      if (Function* callee = C->getCalledFunction()) {
        if (callee->getName().startswith("__soaap_rpc_recv_helper")
            || callee->getName().startswith("__soaap_rpc_recv_sync_helper")) {
          SDEBUG("soaap.analysis.infoflow.rpc", 3, dbgs() << "Receive in sandbox " << S->getName() << ": " << *C << "\n");
          // extract args
          string msgType = "";
          Function* msgHandler = NULL;
          if (GlobalVariable* msgTypeStrGlobal = dyn_cast<GlobalVariable>(C->getArgOperand(1)->stripPointerCasts())) {
            if (ConstantDataArray* msgTypeStrArray = dyn_cast<ConstantDataArray>(msgTypeStrGlobal->getInitializer())) {
              msgType = msgTypeStrArray->getAsCString();
            }
          }
          if (callee->getName().startswith("__soaap_rpc_recv_helper")) {
            msgHandler = dyn_cast<Function>(C->getArgOperand(2)->stripPointerCasts());
          }
          else {
            msgHandler = C->getParent()->getParent();
          }
          receiverToMsgTypeHandler[S][msgType] = msgHandler;
          SDEBUG("soaap.analysis.infoflow.rpc", 3, dbgs() << "msg type: " << msgType << "\n");
          SDEBUG("soaap.analysis.infoflow.rpc", 3, dbgs() << "msg handler: " << msgHandler->getName() << "\n");
        }
      }
    }
  }

  /* 
   * Connect sends to receives and thus build the RPC graph!
   */
  for (map<Sandbox*,SmallVector<CallInst*,16>>::iterator I=senderToCalls.begin(), E=senderToCalls.end(); I!= E; I++) {
    Sandbox* S = I->first;
    SmallVector<CallInst*,16>& calls = I->second;
    for (CallInst* C : calls) {
      // dissect args
      Sandbox* recipient = NULL;
      string msgType = "";
      if (GlobalVariable* recipientStrGlobal = dyn_cast<GlobalVariable>(C->getArgOperand(0)->stripPointerCasts())) {
        if (ConstantDataArray* recipientStrArray = dyn_cast<ConstantDataArray>(recipientStrGlobal->getInitializer())) {
          SDEBUG("soaap.analysis.infoflow.rpc", 3, dbgs() << "Recipient (string): " << recipientStrArray->getAsCString() << "\n");
          recipient = SandboxUtils::getSandboxWithName(recipientStrArray->getAsCString(), sandboxes);
          SDEBUG("soaap.analysis.infoflow.rpc", 3, dbgs() << "Recipient (obtained): " << (recipient == NULL ? "<privileged>" : recipient->getName()) << "\n");
        }
      }
      if (GlobalVariable* msgTypeStrGlobal = dyn_cast<GlobalVariable>(C->getArgOperand(1)->stripPointerCasts())) {
        if (ConstantDataArray* msgTypeStrArray = dyn_cast<ConstantDataArray>(msgTypeStrGlobal->getInitializer())) {
          msgType = msgTypeStrArray->getAsCString();
          SDEBUG("soaap.analysis.infoflow.rpc", 3, dbgs() << "Message type: " << msgType << "\n");
        }
      }
      rpcLinks[S].push_back(RPCCallRecord(C, msgType, recipient, receiverToMsgTypeHandler[recipient][msgType]));
    }
  }

}


void RPCGraph::dump() {
  //map<Sandbox*,SmallVector<RPCCallRecord, 16>>
  //typedef std::tuple<CallInst*,string,Sandbox*,Function*> RPCCallRecord;
  for (map<Sandbox*,SmallVector<RPCCallRecord,16>>::iterator I=rpcLinks.begin(), E=rpcLinks.end(); I!=E; I++) {
    Sandbox* S = I->first;
    SmallVector<RPCCallRecord,16> Calls = I->second;

    for (RPCCallRecord R : Calls) {
      CallInst* Call = get<0>(R);
      Function* Source = Call->getParent()->getParent();
      Sandbox* Dest = get<2>(R);
      Function* Handler = get<3>(R);
      outs() << Source->getName() << " (" << ((S == NULL) ? "<privileged>" : S->getName()) << ") -- " << get<1>(R) << " --> ";
      outs() << ((Dest == NULL) ? "<privileged>" : Dest->getName()) << " (";
      if (Handler == NULL) {
        outs() << "handler missing";
      }
      else {
        outs() << "handled by " << Handler->getName();
      }
      outs() << ")\n";
    }
  }
}
