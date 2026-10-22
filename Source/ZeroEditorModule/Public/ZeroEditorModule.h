#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FExtender;

DECLARE_LOG_CATEGORY_EXTERN(LogZeroEditor, All, All);
class FZeroEditorModuleModule : public IModuleInterface
{
public:

    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    void AddMenu(FMenuBarBuilder& MenuBarBuilder);
    void FillMenu(FMenuBuilder& MenuBuilder);
    void PrintAllAbilities();

private:
    TSharedPtr<FExtender> MenuExtender;
};
