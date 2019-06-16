#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/JITSymbol.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/Orc/IRCompileLayer.h"
#include "llvm/ExecutionEngine/Orc/LambdaResolver.h"
#include "llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/RTDyldMemoryManager.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Mangler.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"

namespace llvm {
namespace orc {

class KaleidoscopeJIT {
   public:
    using ObjLayerT = LegacyRTDyldObjectLinkingLayer;
    using CompileLayerT = LegacyIRCompileLayer<ObjLayerT, SimpleCompiler>;

    KaleidoscopeJIT()
        : Resolver(createLegacyLookupResolver(
              ES,
              [this](const std::string &Name) {
                  return findMangledSymbol(Name);
              },
              [](Error Err) {
                  cantFail(std::move(Err), "lookupFlags failed");
              })),
          TM(EngineBuilder().selectTarget()),
          DL(TM->createDataLayout()),
          ObjectLayer(ES,
                      [this](VModuleKey) {
                          return ObjLayerT::Resources{
                              std::make_shared<SectionMemoryManager>(),
                              Resolver};
                      }),
          CompileLayer(ObjectLayer, SimpleCompiler(*TM)) {
        llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);
    }

    TargetMachine &getTargetMachine() { return *TM; }

    VModuleKey addModule(std::unique_ptr<Module> M) {
        auto K = ES.allocateVModule();
        cantFail(CompileLayer.addModule(K, std::move(M)));
        ModuleKeys.push_back(K);
        return K;
    }

    void removeModule(VModuleKey K) {
        ModuleKeys.erase(find(ModuleKeys, K));
        cantFail(CompileLayer.removeModule(K));
    }

    JITSymbol findSymbol(const std::string Name) {
        return findMangledSymbol(mangle(Name));
    }

   private:
    std::string mangle(const std::string &Name) {
        std::string MangledName;
        {
            raw_string_ostream MangledNameStream(MangledName);
            Mangler::getNameWithPrefix(MangledNameStream, Name, DL);
        }
        return MangledName;
    }

    JITSymbol findMangledSymbol(const std::string &Name) {
        const bool ExportedSymbolsOnly = true;

        for (auto H : make_range(ModuleKeys.rbegin(), ModuleKeys.rend()))
            if (auto Sym =
                    CompileLayer.findSymbolIn(H, Name, ExportedSymbolsOnly))
                return Sym;

        if (auto SymAddr = RTDyldMemoryManager::getSymbolAddressInProcess(Name))
            return JITSymbol(SymAddr, JITSymbolFlags::Exported);
        return nullptr;
    }

    ExecutionSession ES;
    std::shared_ptr<SymbolResolver> Resolver;
    std::unique_ptr<TargetMachine> TM;
    const DataLayout DL;
    ObjLayerT ObjectLayer;
    CompileLayerT CompileLayer;
    std::vector<VModuleKey> ModuleKeys;
};
}  // namespace orc
}  // namespace llvm