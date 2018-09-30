/**
 * @file bindings.c
 * \copybrief bindings.h
 */

/// \cond
#include <regex.h>
#include <strings.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
/// \endcond

#include "bindings.h"
#include "devices.h"
#include "globals.h"
#include "logger.h"


/**
 * Push a binding to the active master stack.
 * Should not be called directly
 * @param chain
 */
static void pushBinding(BoundFunction*chain){
    assert(chain);
    insertHead(getActiveMaster()->activeChains,chain);
}
/**
 * Pop the last added binding from the master stack
 * @return
 */
static BoundFunction* popActiveBinding(){
    BoundFunction*b=getValue(getActiveMaster()->activeChains);
    softDeleteNode(getActiveMaster()->activeChains);
    return b;
}
Node* getActiveBindingNode(){
    return getActiveMaster()->activeChains;
}
Binding* getActiveBinding(){
    if(!isNotEmpty(getActiveBindingNode()))
        return NULL;
    return ((BoundFunction*)getValue(getActiveBindingNode()))->func.chainBindings;
}

int callBoundedFunction(BoundFunction*boundFunction,WindowInfo*winInfo){
///\cond
#define _BF_CASE(TYPE,FUNC_ARG,ARG...)\
    case TYPE: ((void (*)FUNC_ARG)boundFunction->func.func)(ARG);break; \
    case TYPE+1: return ((int (*)FUNC_ARG)boundFunction->func.funcReturnInt)(ARG);
///\endcond

    assert(boundFunction);
    LOG(LOG_LEVEL_TRACE,"calling function %d\n",boundFunction->type);

    BoundFunctionArg arg=boundFunction->arg;

    if(boundFunction->dynamic==2)
        arg.intArg=callBoundedFunction(boundFunction->arg.voidArg, winInfo);

    else if(boundFunction->dynamic==1)
        arg.voidArg=winInfo;

    switch(boundFunction->type){
        case UNSET:
            LOG(LOG_LEVEL_WARN,"calling unset function; nothing is happening\n");
            break;
        case CHAIN:
            assert(!boundFunction->func.chainBindings->endChain);
            startChain(boundFunction);
            break;
        case CHAIN_AUTO:
            startChain(boundFunction);
            return callBoundedFunction(&boundFunction->func.chainBindings->boundFunction, winInfo);
        case FUNC_AND:
            for(int i=0;i<boundFunction->arg.intArg;i++)
                if(!callBoundedFunction(&boundFunction->func.boundFunctionArr[i],winInfo))
                    return 0;
            return 1;
        case FUNC_OR:
            for(int i=0;i<boundFunction->arg.intArg;i++)
                if(callBoundedFunction(&boundFunction->func.boundFunctionArr[i],winInfo))
                    return 1;
            return 0;
        _BF_CASE(NO_ARGS,(void))
        _BF_CASE(INT_ARG,(int),arg.intArg)
        _BF_CASE(WIN_ARG,(WindowInfo*),arg.voidArg)
        _BF_CASE(WIN_INT_ARG,(WindowInfo*,int),winInfo,arg.intArg)
        _BF_CASE(VOID_ARG,(void*),arg.voidArg)

    }
    return 1;
}
///\cond
#define FOR_EACH_CHAIN(CODE...){ int index=0;\
    while(1){CODE;\
            if(chain[index++].endChain) \
                break; \
        } \
    }
///\endcond
void startChain(BoundFunction *boundFunction){
    Binding*chain=boundFunction->func.chainBindings;
    int mask=boundFunction->arg.intArg;
    LOG(LOG_LEVEL_TRACE,"starting chain. mask:%d\n",mask);
    if(mask)
        grabDevice(getDeviceIDByMask(mask),mask);
    FOR_EACH_CHAIN(
        if(!chain[index].detail)
            initBinding(&chain[index]);
        grabBinding(&chain[index])
    );
    pushBinding(boundFunction);
}
void endChain(){
    BoundFunction *boundFunction=popActiveBinding();
    Binding*chain=boundFunction->func.chainBindings;
    int mask=boundFunction->arg.intArg;
    if(mask)
        ungrabDevice(getDeviceIDByMask(mask));
    FOR_EACH_CHAIN(ungrabBinding(&chain[index]));
}
int getIDOfBindingTarget(Binding*binding){
    return binding->targetID==-1?
            isKeyboardMask(binding->mask)?
                getActiveMasterKeyboardID():getActiveMasterPointerID():
            binding->targetID;
}



int doesBindingMatch(Binding*binding,int detail,int mods,int mask){
    return (binding->mask & mask || mask==0) && (binding->mod == WILDCARD_MODIFIER|| binding->mod==mods) &&
            (binding->detail==0||binding->detail==detail);
}
int checkBindings(int keyCode,int mods,int mask,WindowInfo*winInfo){

    mods&=~IGNORE_MASK;

    LOG(LOG_LEVEL_TRACE,"key detected code: %d mod: %d mask: %d\n",keyCode,mods,mask);

    Binding* chainBinding=getActiveBinding();
    int bindingTriggered=0;
    //TODO simplify logic
    while(chainBinding){
        int i;
        for(i=0;;i++){
            if(doesBindingMatch(&chainBinding[i],keyCode,mods,mask)){
                LOG(LOG_LEVEL_TRACE,"Calling chain binding\n");
                int result=callBoundedFunction(&chainBinding[i].boundFunction,winInfo);
                if(chainBinding[i].endChain){
                    //chain has ended
                    endChain();
                }
                if(!passThrough(result,chainBinding[i].passThrough))
                    return 0;
                bindingTriggered+=1;
                if(chainBinding[i].endChain){
                    break;
                }
            }
            if(chainBinding[i].endChain){
                if(!chainBinding[i].noEndOnPassThrough){
                    LOG(LOG_LEVEL_DEBUG,"chain is ending because external key was pressed\n");
                    endChain();
                }
                break;
            }
        }
        if(chainBinding==getActiveBinding())
            break;
        else chainBinding=getActiveBinding();
    }
    Node*n=deviceBindings;
    FOR_EACH_CIRCULAR(n,
            Binding*binding=getValue(n);
        if(doesBindingMatch(binding,keyCode,mods,mask)){
            bindingTriggered+=1;
            if(!passThrough(callBoundedFunction(&binding->boundFunction,winInfo),binding->passThrough))
               return 0;
        }
    )
    LOG(LOG_LEVEL_TRACE,"Triggered %d bindings\n",bindingTriggered);
    return bindingTriggered;
}

int doesStringMatchRule(Rule*rule,char*str){
    if(!str)
        return 0;
    if(rule->ruleTarget & LITERAL){
        if(rule->ruleTarget & CASE_INSENSITIVE)
            return strcasecmp(rule->literal,str)==0;
        else return strcmp(rule->literal,str)==0;
    }
    else{
        assert(rule->regexExpression!=NULL);
        return !regexec(rule->regexExpression, str, 0, NULL, 0);
    }
}
int doesWindowMatchRule(Rule *rules,WindowInfo*winInfo){
    if(!rules->literal)
        return 1;
    if(!winInfo)return 0;
    return (rules->ruleTarget & CLASS) && doesStringMatchRule(rules,winInfo->className) ||
        (rules->ruleTarget & RESOURCE) && doesStringMatchRule(rules,winInfo->instanceName) ||
        (rules->ruleTarget & TITLE) && doesStringMatchRule(rules,winInfo->title) ||
        (rules->ruleTarget & TYPE) && doesStringMatchRule(rules,winInfo->typeName);
}
int passThrough(int result,PassThrough pass){
    switch(pass){
        case NO_PASSTHROUGH:
            return 0;
        case ALWAYS_PASSTHROUGH:
            return 1;
        case PASSTHROUGH_IF_TRUE:
            return result;
        case PASSTHROUGH_IF_FALSE:
            return !result;
    }
    LOG(LOG_LEVEL_WARN,"invalid pass through option %d\n",pass);
    return 0;
}
int applyRule(Rule*rule,WindowInfo*winInfo){
    assert(rule);
    if(doesWindowMatchRule(rule,winInfo))
        return passThrough(callBoundedFunction(&rule->onMatch,winInfo),rule->passThrough);
    return 1;
}

int applyRules(Node* head,WindowInfo*winInfo){
    assert(head);
    FOR_EACH_CIRCULAR(head,
        Rule *rule=getValue(head);
        assert(rule);
        if(!applyRule(rule,winInfo))
            return 0;
        )
    return 1;
}
void addBindings(Binding*bindings,int num){
    for(int i=0;i<num;i++)
        insertTail(deviceBindings, &bindings[i]);
}
void addBinding(Binding*binding){
    addBindings(binding,1);
}
/**
 * Convience method for grabBinding() and ungrabBinding()
 * @param binding
 * @param ungrab
 * @return
 */
int _grabUngrabBinding(Binding*binding,int ungrab){
    if(!binding->noGrab){
        if (!ungrab)
            return grabDetail(getIDOfBindingTarget(binding),binding->detail,binding->mod,binding->mask);
        else
            return ungrabDetail(getIDOfBindingTarget(binding), binding->detail,binding->mod, isKeyboardMask(binding->mask));
    }
    else
        return 1;
}
int grabBinding(Binding*binding){
    return _grabUngrabBinding(binding,0);
}
int ungrabBinding(Binding*binding){
    return _grabUngrabBinding(binding,1);
}
int getKeyCode(int keysym){
    return XKeysymToKeycode(dpy, keysym);
}
void initBinding(Binding*binding){
    if(binding->targetID==BIND_TO_ALL_MASTERS)
        binding->targetID=XCB_INPUT_DEVICE_ALL_MASTER;
    else if(binding->targetID==BIND_TO_ALL_DEVICES)
        binding->targetID=XCB_INPUT_DEVICE_ALL;

    if(!binding->mask)
        binding->mask=DEFAULT_BINDING_MASKS;
    if(binding->buttonOrKey && isKeyboardMask(binding->mask))
        binding->detail=getKeyCode(binding->buttonOrKey);
    else binding->detail=binding->buttonOrKey;

    assert(binding->detail||!binding->buttonOrKey);

}

/*if(chain!=getActiveMaster()->lastBindingTriggered){
        Master *m=getActiveMaster();
        clearWindowCache(m);
        getActiveMaster()->lastBindingTriggered=chain;
    }*/