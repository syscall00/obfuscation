#define DEBUG_TYPE "StringObfuscation"

#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/IR/ValueSymbolTable.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include <typeinfo>
#include <string>
#include <strstream>
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/CallSite.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/CryptoUtils.h"
#include "llvm/Transforms/Obfuscation/StringObfuscation.h"
#include "llvm/IR/Instructions.h"

#define CAP 20

STATISTIC(GlobalsEncoded,"Number of strings encoded");
using namespace std;
using namespace llvm;

namespace {

    struct ProtectedString {
    public:
        GlobalVariable *Glob;
        unsigned int Key;
        unsigned int Length;

        ProtectedString(GlobalVariable *Glob, unsigned int Key, unsigned int Length) : Glob(Glob), Key(Key),
                                                                                       Length(Length) {}

    };

    struct StringObfuscator : public ModulePass {
        static char ID;
        bool Flag;
        StringObfuscator() : ModulePass(ID) { }

        StringObfuscator(bool flag) : ModulePass(ID) { this->Flag = flag; StringObfuscator();}


        bool runOnModule(Module &M);

        Function *insertDecode(Module &M);
        void insertArrays(Module &M, vector<ProtectedString*> encVars);
        Function *insertDecodeWrapper(Module &M, vector<ProtectedString *> &strings, Function *DecodeFunc);

        void callDecodeWrapper(Function *F, Function *DecodeWrapperFunc);
    };
}


/**
 * Utility function. XOR encipher a string given a specific key
 * @param Data  plaintext string
 * @param Length    length of the string
 * @param Key   key for the encryption
 * @return      ciphered string
 */
char *encryptString(const char *Data, unsigned int Length, unsigned int Key) {

    char *NewData = (char *) malloc(Length);
    for (unsigned int i = 0; i < Length; i++) {
        NewData[i] = Data[i] ^ Key;
    }
    return NewData;
}

/**
 * Encrypt all the strings in the module and get them
 * @param M module to process
 * @return vector of ProtectedString pointers
 */
vector<ProtectedString *> getStrings(Module &M) {

    vector <ProtectedString*> encVars;

    srand((unsigned) time(NULL));

    auto &Ctx = M.getContext();

    for (GlobalVariable &Glob : M.globals()) {
        // Ignore external globals & uninitialized globals.
        if (!Glob.hasInitializer() || Glob.hasExternalLinkage())
            continue;

        // get the value of the globalVariable
        Constant *Initializer = Glob.getInitializer();

        // check if it is a string
        if (isa<ConstantDataArray>(Initializer)) {
            auto CDA = cast<ConstantDataArray>(Initializer);
            if (!CDA->isString())
                continue;

            // extract the string
            StringRef StrVal = CDA->getAsString();
            const char *Data = StrVal.begin();
            const int Size = StrVal.size();
            const int Key = rand() % 256;


            // create encoded string variable. Constants are immutable so should be overwritten with a new Constant.
            char *NewData = encryptString(Data, Size, Key);
            Constant *NewConst = ConstantDataArray::getString(Ctx, StringRef(NewData, Size), false);

            // Overwrite the global value
            Glob.setInitializer(NewConst);


            encVars.push_back(new ProtectedString(&Glob, Key, Size));
            GlobalsEncoded++;
            // Save the global variable as non constant because during runtime we have to edit its value
            Glob.setConstant(false);
        }
    }

    return encVars;

}

/**
 * Create three arrays and insert them in the module:
 * <ul>
 * <li>encStrings: array containing all strings to be obfuscated;</li>
 * <li>encLength: array contain all length of each string to be obfuscated;</li>
 * <li>encKey: array containing all keys used to cipher each string;</li>
 * </ul>
 * @param M
 * @param encVars
 */
void StringObfuscator::insertArrays(Module &M, vector<ProtectedString*> encVars) {
    auto &Ctx = M.getContext();

    ArrayType *TyStrArray = ArrayType::get(Type::getInt8PtrTy(Ctx), encVars.size());
    ArrayType *TyIntArray = ArrayType::get(Type::getInt32Ty(Ctx), encVars.size());
    ArrayType *TyCharArray = ArrayType::get(Type::getInt8Ty(Ctx), encVars.size());

    // add the global arrays
    M.getOrInsertGlobal("encStrings", TyStrArray);
    M.getOrInsertGlobal("encLength", TyIntArray);
    M.getOrInsertGlobal("encKeys", TyCharArray);

    // get the just created arrays
    auto encStrings = M.getNamedGlobal("encStrings");
    auto encLength = M.getNamedGlobal("encLength");
    auto encKey = M.getNamedGlobal("encKeys");

    vector <Constant*> strVec;
    vector <Constant*> lenVec;
    vector <Constant*> keyVec;

    ConstantInt *cons = ConstantInt::get(Ctx, APInt(32, StringRef("0"), 10));
    std::vector <Constant *> vct;
    vct.push_back(cons);
    vct.push_back(cons);

    for (ProtectedString *encs : encVars) {
        // populate the arrays
        strVec.push_back(ConstantExpr::getInBoundsGetElementPtr(encs->Glob->getValueType(), encs->Glob, vct));
        lenVec.push_back(ConstantInt::get(Ctx, APInt(32, StringRef(std::to_string(encs->Length)), 10)));
        keyVec.push_back(ConstantInt::get(Ctx, APInt(8, StringRef(std::to_string(encs->Key)), 10)));
    }
    // initialize the arrays
    encStrings->setInitializer(ConstantArray::get(TyStrArray, strVec));
    encLength->setInitializer(ConstantArray::get(TyIntArray, lenVec));
    encKey->setInitializer(ConstantArray::get(TyCharArray, keyVec));

}

/**
 * Override runOnModule from ModulePass class. It is the entry point of the transformation pass
 * @param M module to process
 * @return result of the obfuscation
 */
bool StringObfuscator::runOnModule(Module &M) {
    Function *MainFunc = M.getFunction("main");

    if (Flag && MainFunc) {

        vector < ProtectedString * > encVars = getStrings(M);

        // if there are more than CAP strings, then create the arrays needed for in-code loop
        if (encVars.size() >= CAP)
            insertArrays(M, encVars);

        Function *DecodeFunc = insertDecode(M);
        Function *DecodeStub = insertDecodeWrapper(M, encVars, DecodeFunc);

        callDecodeWrapper(MainFunc, DecodeStub);
    }

    return Flag;
}

/**
 * Given a module, insert a decode function based on XOR encryption.
 * @param M   module to process
 * @return  decodeFunction pointer
 */
Function *StringObfuscator::insertDecode(Module &M) {
    auto &Ctx = M.getContext();

    FunctionType *FT =
            FunctionType::get(Type::getVoidTy(Ctx),
                              {Type::getInt8PtrTy(Ctx, 8), IntegerType::get(Ctx, 32), Type::getInt8Ty(Ctx)},
                              false);

    Function *F = Function::Create(FT, Function::ExternalLinkage, "decode", &M);

    F->setCallingConv(CallingConv::C);

    // generate BasicBlocks
    BasicBlock *bbCheck = BasicBlock::Create(Ctx, "while.check", F);
    BasicBlock *bbBody = BasicBlock::Create(Ctx, "while.body", F);
    BasicBlock *bbEnd = BasicBlock::Create(Ctx, "while.end", F);

    // get the 3 arguments
    Function::arg_iterator Args = F->arg_begin();
    Value *StringArg, *LengthArg, *KeyArg;

    StringArg = dyn_cast<Value>(Args++);
    LengthArg = dyn_cast<Value>(Args++);
    KeyArg = dyn_cast<Value>(Args++);

    IRBuilder<> *Builder = new IRBuilder<>(bbCheck);

    // while (p && length > 0)
    auto *var3 = Builder->CreateICmpNE(StringArg, Constant::getNullValue(Type::getInt8PtrTy(Ctx, 8)));
    auto *var4 = Builder->CreateICmpSGT(LengthArg, ConstantInt::get(IntegerType::get(Ctx, 32), 0));
    auto *var5 = Builder->CreateAnd(var4, var3);

    // conditional jump
    Builder->CreateCondBr(var5, bbBody, bbEnd);

    Builder->SetInsertPoint(bbBody);
    PHINode *phi_var7 = Builder->CreatePHI(Type::getInt8PtrTy(Ctx, 8), 2);
    PHINode *phi_var8 = Builder->CreatePHI(Type::getInt32Ty(Ctx), 2);
    auto *var9 = Builder->CreateNSWAdd(phi_var8, ConstantInt::getSigned(IntegerType::get(Ctx, 32), -1));
    auto *var10 = Builder->CreateGEP(phi_var7, ConstantInt::get(IntegerType::get(Ctx, 64), 1));


    // p[i] = p[i] ^ key
    auto *var11 = Builder->CreateLoad(phi_var7);
    auto *var12 = Builder->CreateXor(var11, KeyArg);
    Builder->CreateStore(var12, phi_var7);

    auto *var13 = Builder->CreateICmpSGT(phi_var8, ConstantInt::get(IntegerType::get(Ctx, 32), 1));
    Builder->CreateCondBr(var13, bbBody, bbEnd);

    Builder->SetInsertPoint(bbEnd);
    // return
    Builder->CreateRetVoid();

    //%7 = phi i8* [ %10, %6 ], [ %0, %2 ]
    phi_var7->addIncoming(StringArg, bbCheck);
    phi_var7->addIncoming(var10, bbBody);
    //%8 = phi i32 [ %9, %6 ], [ %1, %2 ]
    phi_var8->addIncoming(LengthArg, bbCheck);
    phi_var8->addIncoming(var9, bbBody);

    return F;
}

/**
 * Create and insert decode wrapper which will be decipher all the
 * strings in the module
 * @param M module to process
 * @param strings vector of strings encrypted
 * @param DecodeFunc
 * @return decodeWrapper inserted
 */
Function *StringObfuscator::insertDecodeWrapper(Module &M, vector<ProtectedString *> &encVars, Function *DecodeFunc) {
    auto &Ctx = M.getContext();
    // Add decodeWrapper function
    FunctionType *FT = FunctionType::get(Type::getVoidTy(Ctx), false);

    Function *DecodeWrapperFunc = Function::Create(FT, Function::ExternalLinkage, "dcode_wrap", &M);
    BasicBlock *EPP = BasicBlock::Create(Ctx, "entry", DecodeWrapperFunc);

    IRBuilder<> Builder(EPP);

    /**
     * IF: there are more than CAP strings in the code, will be looped three arrays
     * containing the strings, their length and the keys used to encode them.
     * code generated:
     * for(int i = num_strings; i >= 0; i++)
     *      decode(encStrings[i], encLength[i], encKeys[i]);
     */
    if (encVars.size() >= CAP) {
        // initialize a local variable with length of the arrays
        auto size = Builder.CreateAlloca(Type::getInt32Ty(Ctx));
        Builder.CreateStore(ConstantInt::get(IntegerType::get(Ctx, 32), encVars.size()), size);

        // create entry block
        BasicBlock *BB = BasicBlock::Create(Ctx, "for.cond", DecodeWrapperFunc);
        Builder.CreateBr(BB);

        Builder.SetInsertPoint(BB);
        auto ld = Builder.CreateLoad(size);
        auto *var4 = Builder.CreateICmpSGE(ld, ConstantInt::get(IntegerType::get(Ctx, 32), 0));
        BasicBlock *bb6 = BasicBlock::Create(Ctx, "for.body", DecodeWrapperFunc);
        BasicBlock *bb7 = BasicBlock::Create(Ctx, "for.incr", DecodeWrapperFunc);
        BasicBlock *bb14 = BasicBlock::Create(Ctx, "for.end", DecodeWrapperFunc);
        Builder.CreateCondBr(var4, bb6, bb14);

        // get the array with strings
        auto global = M.getNamedGlobal("encStrings");
        auto lengths = M.getNamedGlobal("encLength");
        auto keys = M.getNamedGlobal("encKeys");


        Value *cons = ConstantInt::get(Ctx, APInt(64, StringRef("0"), 10));
        std::vector < Value * > vct;
        vct.push_back(cons);

        // for body
        Builder.SetInsertPoint(bb6);
        vct.push_back(Builder.CreateLoad(size));

        auto load_str = Builder.CreateLoad(Type::getInt8PtrTy(Ctx), Builder.CreateInBoundsGEP(global, vct));
        auto *str_ptr_ = Builder.CreatePointerCast(load_str, Type::getInt8PtrTy(Ctx, 8));

        auto load_length = Builder.CreateLoad(Type::getInt32Ty(Ctx), Builder.CreateInBoundsGEP(lengths, vct));
        auto load_key = Builder.CreateLoad(Type::getInt8Ty(Ctx), Builder.CreateInBoundsGEP(keys, vct));


        Builder.CreateCall(DecodeFunc, {str_ptr_, load_length, load_key});

        Builder.CreateBr(bb7);

        Builder.SetInsertPoint(bb7);
        // load
        ld = Builder.CreateLoad(size);
        auto *var9 = Builder.CreateNSWAdd(ld, ConstantInt::getSigned(IntegerType::get(Ctx, 32), -1));

        Builder.CreateStore(var9, size);
        Builder.CreateBr(BB);

        Builder.SetInsertPoint(bb14);
        Builder.CreateRetVoid();

    }
    /**
     * ELSE: manually load every single string and call decode func on each of it.
     * NOTE: This solution in not scalable and should be used only for few
     * strings.
     * code generated:
     * decode(str_0, len_0, key_0);
     * decode(str_1, len_1, key_1);
     * ...
     * decode(str_i, len_i, key_i);
     *
     * with i = len(encVars);
     */
    else {
        // Add calls to decode every encoded global
        for (ProtectedString *encs : encVars) {
            // load string
            auto *StrPtr = Builder.CreatePointerCast(encs->Glob, Type::getInt8PtrTy(Ctx, 8));
            // create int with length
            Value *le = ConstantInt::get(IntegerType::getInt32Ty(Ctx), encs->Length);
            // create char with key
            Value *Key = ConstantInt::get(IntegerType::getInt8Ty(Ctx), encs->Key);

            // call function with parameters
            Builder.CreateCall(DecodeFunc, {StrPtr, le, Key});
        }
        Builder.CreateRetVoid();

    }

    return DecodeWrapperFunc;
}


/**
 * Add a call to decodeWrapperFunc
 * @param F function in which will be added the decode wrapper
 * @param DecodeWrapperFunc decode wrapper function
 */
void StringObfuscator::callDecodeWrapper(Function *F, Function *DecodeWrapperFunc) {
    auto &Ctx = F->getContext();
    BasicBlock &EntryBlock = F->getEntryBlock();

    // Create new block
    BasicBlock *NewBB = BasicBlock::Create(Ctx, "decodeWrapper", EntryBlock.getParent(), &EntryBlock);
    IRBuilder<> Builder(NewBB);
    // Call stub func instruction
    Builder.CreateCall(DecodeWrapperFunc);
    // Jump to original entry block
    Builder.CreateBr(&EntryBlock);
}


char StringObfuscator::ID = 0;
static RegisterPass <StringObfuscator> X("GVDiv", "Global variable (i.e., const char*) diversification pass", false,
                                         true);
Pass *llvm::createStringObfuscator(bool flag) { return new StringObfuscator(flag); }
