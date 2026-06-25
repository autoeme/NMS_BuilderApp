// Часть класса SNMSBuilderUI (разнесение по TU, Часть C плана).
// Включения общие для всех TU класса — см. SNMSBuilderUI_Internal.h.
#include "SNMSBuilderUI_Internal.h"

#define LOCTEXT_NAMESPACE "NMSBuilder"

void SNMSBuilderUI::SMRefreshAccounts()
{
    SMAccounts = FNMSSaveFile::GetAccounts();
    SMSlots.Reset(); SMBases.Reset();
    SMAccountIdx = INDEX_NONE; SMSlotIdx = INDEX_NONE;

    // автовыбор: если один аккаунт — берём его
    if (SMAccounts.Num() == 1)
        SMSelectAccount(0);
    else if (SMAccounts.Num() > 1)
    {
        // автовыбор аккаунта с самым свежим сейвом
        int32 BestAcc = 0; double BestTime = -1;
        for (int32 i=0;i<SMAccounts.Num();++i)
        {
            TArray<FNMSSaveSlot> S = FNMSSaveFile::GetSaveSlots(SMAccounts[i]);
            int32 L = FNMSSaveFile::GetLatestSlotIndex(S);
            if (L>=0 && S[L].LastModified > BestTime) { BestTime=S[L].LastModified; BestAcc=i; }
        }
        SMSelectAccount(BestAcc);
    }
    if (SMStatusText.IsValid())
        SMStatusText->SetText(FText::FromString(FString::Printf(
            TEXT("Аккаунтов: %d"), SMAccounts.Num())));
}

void SNMSBuilderUI::SMSelectAccount(int32 Idx)
{
    if (!SMAccounts.IsValidIndex(Idx)) return;
    SMAccountIdx = Idx;
    SMSlots = FNMSSaveFile::GetSaveSlots(SMAccounts[Idx]);
    SMBases.Reset(); SMSlotIdx = INDEX_NONE;

    // АВТОВЫБОР последнего активного слота
    const int32 Latest = FNMSSaveFile::GetLatestSlotIndex(SMSlots);
    if (Latest >= 0) SMSelectSlot(Latest);
}

void SNMSBuilderUI::SMSelectSlot(int32 Idx)
{
    if (!SMSlots.IsValidIndex(Idx)) return;
    SMSlotIdx = Idx;
    SMBases.Reset();

    const FString& Hg = SMSlots[Idx].PrimaryHg.IsEmpty()
        ? SMSlots[Idx].SecondaryHg : SMSlots[Idx].PrimaryHg;
    FNMSSaveFile SF(Hg);
    FString Err;
    if (SF.Load(Err))
    {
        SMBases = SF.ExtractBases();
        UE_LOG(LogTemp, Log, TEXT("NMS SaveMgr: слот %d, баз: %d"),
            SMSlots[Idx].SlotNumber, SMBases.Num());
    }
    else
        UE_LOG(LogTemp, Error, TEXT("NMS SaveMgr: %s"), *Err);
    SMRebuildBaseList();
}

void SNMSBuilderUI::SMLoadBaseIntoEditor(int32 BaseIndex)
{
    if (SMSlotIdx == INDEX_NONE || !SMSlots.IsValidIndex(SMSlotIdx)) return;
    const FString& Hg = SMSlots[SMSlotIdx].PrimaryHg.IsEmpty()
        ? SMSlots[SMSlotIdx].SecondaryHg : SMSlots[SMSlotIdx].PrimaryHg;
    FNMSSaveFile SF(Hg);
    FString Err;
    if (!SF.Load(Err)) { UE_LOG(LogTemp,Error,TEXT("NMS SaveMgr load: %s"),*Err); return; }

    // достаём JSON базы по индексу и грузим её Objects в редактор
    TSharedPtr<FJsonObject> Root = SF.GetJson();
    if (!Root.IsValid()) return;
    const TSharedPtr<FJsonObject>* BC; if(!Root->TryGetObjectField(TEXT("BaseContext"),BC)) return;
    const TSharedPtr<FJsonObject>* PSD; if(!(*BC)->TryGetObjectField(TEXT("PlayerStateData"),PSD)) return;
    const TArray<TSharedPtr<FJsonValue>>* Bases;
    if(!(*PSD)->TryGetArrayField(TEXT("PersistentPlayerBases"),Bases)) return;
    if(!Bases->IsValidIndex(BaseIndex)) return;

    TSharedPtr<FJsonObject> Base = (*Bases)[BaseIndex]->AsObject();
    if(!Base.IsValid()) return;

    // имя/адрес базы -> в свойства
    Base->TryGetStringField(TEXT("Name"), CurrentBaseName);

    // собираем JSON {Objects:[...]} и грузим через существующий импорт
    TSharedPtr<FJsonObject> Wrap = MakeShared<FJsonObject>();
    const TArray<TSharedPtr<FJsonValue>>* Objs;
    if (Base->TryGetArrayField(TEXT("Objects"), Objs))
    {
        Wrap->SetArrayField(TEXT("Objects"), *Objs);
        FString JsonStr;
        TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&JsonStr);
        FJsonSerializer::Serialize(Wrap.ToSharedRef(), W);
        // грузим через тот же путь, что Import from Clipboard
        UNMSBaseManager* Mgr = NewObject<UNMSBaseManager>();
        Mgr->AddToRoot();
        if (Mgr->ImportFromString(JsonStr))
            SpawnFromManager(Mgr);
        Mgr->RemoveFromRoot();
        UE_LOG(LogTemp, Warning, TEXT("NMS SaveMgr: база '%s' загружена (%d деталей)"),
            *CurrentBaseName, Objs->Num());
    }
}

void SNMSBuilderUI::SMRebuildBaseList()
{
    if (!SMBaseListBox.IsValid()) return;
    SMBaseListBox->ClearChildren();

    for (int32 i = 0; i < SMBases.Num(); ++i)
    {
        const FNMSSaveBaseInfo& B = SMBases[i];
        // фильтр Corvette/Основа
        const bool bCorv = (B.BaseType == TEXT("PlayerShipBase"));
        if (bCorv != SMShowCorvettes) continue;
        // фильтр по поиску
        if (!SMSearchText.IsEmpty() &&
            !B.BaseName.Contains(SMSearchText)) continue;

        const int32 BaseIdx = B.BaseIndex;
        const FString Label = B.BaseName.IsEmpty()
            ? FString::Printf(TEXT("(без имени) [%d]"), BaseIdx) : B.BaseName;
        const FString Tip = FString::Printf(TEXT("Деталей: %d  |  Тип: %s  |  Адрес: %s"),
            B.PartCount, *B.BaseType, *B.GalacticAddress);

        const FNMSSaveBaseInfo BCopy = B;
        SMBaseListBox->AddSlot().AutoHeight().Padding(2,1)
        [
            SNew(SButton)
            .ToolTip(SMMakeBaseTooltip(BCopy))   // превью с ФОТО при наведении
            .OnClicked_Lambda([this, BaseIdx]() {
                SMLoadBaseIntoEditor(BaseIdx);
                return FReply::Handled();
            })
            .Content()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
                [ SNew(STextBlock).Text(FText::FromString(Label)) ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [ SNew(STextBlock)
                    .Text(FText::FromString(FString::Printf(TEXT("  %d дет."), B.PartCount)))
                    .ColorAndOpacity(FSlateColor(FLinearColor(0.6f,0.6f,0.6f))) ]
                // кнопка привязки скриншота
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4,0,0,0)
                [ SNew(SButton)
                    .ToolTipText(FText::FromString(TEXT("Привязать скриншот базы")))
                    .Text(FText::FromString(SMBaseScreens.Contains(B.BaseName) ? TEXT("\u0444\u043e\u0442\u043e\u2713") : TEXT("\u0444\u043e\u0442\u043e")))
                    .OnClicked_Lambda([this, BName=B.BaseName]() {
                        SMPickScreenshot(BName);
                        return FReply::Handled();
                    }) ]
            ]
        ];
    }
    if (SMStatusText.IsValid())
    {
        int32 Shown = 0;
        for (const auto& B : SMBases)
            if (((B.BaseType==TEXT("PlayerShipBase"))==SMShowCorvettes) &&
                (SMSearchText.IsEmpty()||B.BaseName.Contains(SMSearchText))) Shown++;
        SMStatusText->SetText(FText::FromString(FString::Printf(
            TEXT("%s: %d"), SMShowCorvettes?TEXT("Корветов"):TEXT("Баз"), Shown)));
    }
}

TSharedRef<SWidget> SNMSBuilderUI::BuildSaveManagerPanel()
{
    // загрузка привязок скриншотов + аккаунтов
    SMLoadScreenLinks();
    SMRefreshAccounts();

    return SNew(SVerticalBox)
    // строка: заголовок + кнопка обновить
    + SVerticalBox::Slot().AutoHeight().Padding(4,2)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
        [ SNew(STextBlock).Text(FText::FromString(TEXT("SAVE MANAGER")))
            .ColorAndOpacity(FSlateColor(FLinearColor(1.f,0.78f,0.18f))) ]
        + SHorizontalBox::Slot().AutoWidth()
        [ SNew(SButton).Text(FText::FromString(TEXT("Обновить")))
            .OnClicked_Lambda([this](){ SMRefreshAccounts(); SMRebuildBaseList(); return FReply::Handled(); }) ]
    ]
    // Select Account
    + SVerticalBox::Slot().AutoHeight().Padding(4,2)
    [
        SNew(SComboButton)
        .ButtonContent()[ SNew(STextBlock).Text_Lambda([this](){
            return FText::FromString(SMAccounts.IsValidIndex(SMAccountIdx)
                ? FPaths::GetCleanFilename(SMAccounts[SMAccountIdx])
                : TEXT("Select Account")); }) ]
        .OnGetMenuContent_Lambda([this](){
            FMenuBuilder MB(true, nullptr);
            for (int32 i=0;i<SMAccounts.Num();++i){
                const int32 Idx=i;
                MB.AddMenuEntry(FText::FromString(FPaths::GetCleanFilename(SMAccounts[i])),
                    FText::GetEmpty(), FSlateIcon(), FUIAction(FExecuteAction::CreateLambda(
                        [this,Idx](){ SMSelectAccount(Idx); SMRebuildBaseList(); })));
            }
            return MB.MakeWidget();
        })
    ]
    // Select Slot
    + SVerticalBox::Slot().AutoHeight().Padding(4,2)
    [
        SNew(SComboButton)
        .ButtonContent()[ SNew(STextBlock).Text_Lambda([this](){
            return FText::FromString(SMSlots.IsValidIndex(SMSlotIdx)
                ? FString::Printf(TEXT("Slot %d"), SMSlots[SMSlotIdx].SlotNumber)
                : TEXT("Select Save Slot")); }) ]
        .OnGetMenuContent_Lambda([this](){
            FMenuBuilder MB(true, nullptr);
            for (int32 i=0;i<SMSlots.Num();++i){
                const int32 Idx=i;
                MB.AddMenuEntry(FText::FromString(FString::Printf(TEXT("Slot %d"), SMSlots[i].SlotNumber)),
                    FText::GetEmpty(), FSlateIcon(), FUIAction(FExecuteAction::CreateLambda(
                        [this,Idx](){ SMSelectSlot(Idx); })));
            }
            return MB.MakeWidget();
        })
    ]
    // переключатель Основа / Корветы
    + SVerticalBox::Slot().AutoHeight().Padding(4,2)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1.f)
        [ SNew(SButton).Text(FText::FromString(TEXT("Основа")))
            .OnClicked_Lambda([this](){ SMShowCorvettes=false; SMRebuildBaseList(); return FReply::Handled(); }) ]
        + SHorizontalBox::Slot().FillWidth(1.f)
        [ SNew(SButton).Text(FText::FromString(TEXT("Корветы")))
            .OnClicked_Lambda([this](){ SMShowCorvettes=true; SMRebuildBaseList(); return FReply::Handled(); }) ]
    ]
    // поиск
    + SVerticalBox::Slot().AutoHeight().Padding(4,2)
    [
        SNew(SEditableTextBox)
        .HintText(FText::FromString(TEXT("Поиск базы...")))
        .OnTextChanged_Lambda([this](const FText& T){ SMSearchText=T.ToString(); SMRebuildBaseList(); })
    ]
    // статус
    + SVerticalBox::Slot().AutoHeight().Padding(4,2)
    [ SAssignNew(SMStatusText, STextBlock).Text(FText::FromString(TEXT(""))) ]
    // список баз (прокрутка)
    + SVerticalBox::Slot().FillHeight(1.f).Padding(4,2)
    [
        SNew(SScrollBox)
        + SScrollBox::Slot()[ SAssignNew(SMBaseListBox, SVerticalBox) ]
    ];
}

// ============ СКРИНШОТЫ БАЗ (превью при наведении) ============
void SNMSBuilderUI::SMLoadScreenLinks()
{
    SMBaseScreens.Reset();
    const FString F = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("NMSUser/base_screenshots.json"));
    FString Raw;
    if (!FFileHelper::LoadFileToString(Raw, *F)) return;
    TSharedPtr<FJsonObject> Obj;
    TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Raw);
    if (FJsonSerializer::Deserialize(R, Obj) && Obj.IsValid())
        for (const auto& Pair : Obj->Values)
        {
            FString V; if (Pair.Value->TryGetString(V))
                SMBaseScreens.Add(FString(Pair.Key), V);
        }
}

void SNMSBuilderUI::SMSaveScreenLinks()
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    for (const auto& Pair : SMBaseScreens)
        Obj->SetStringField(Pair.Key, Pair.Value);
    FString Out;
    TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Obj.ToSharedRef(), W);
    const FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NMSUser"));
    IFileManager::Get().MakeDirectory(*Dir, true);
    FFileHelper::SaveStringToFile(Out, *FPaths::Combine(Dir, TEXT("base_screenshots.json")));
}

const FSlateBrush* SNMSBuilderUI::SMGetScreenBrush(const FString& BaseName)
{
    const FString* PathP = SMBaseScreens.Find(BaseName);
    if (!PathP || PathP->IsEmpty() || !FPaths::FileExists(*PathP)) return nullptr;

    if (TSharedPtr<FSlateDynamicImageBrush>* Cached = SMScreenBrushes.Find(BaseName))
        return Cached->Get();

    // загружаем картинку как brush (превью ~320x180)
    TSharedPtr<FSlateDynamicImageBrush> Brush =
        MakeShareable(new FSlateDynamicImageBrush(FName(**PathP), FVector2D(320, 180)));
    SMScreenBrushes.Add(BaseName, Brush);
    return Brush.Get();
}

void SNMSBuilderUI::SMPickScreenshot(const FString& BaseName)
{
    IDesktopPlatform* DP = FDesktopPlatformModule::Get();
    if (!DP) return;
    const void* H = nullptr;
    if (FSlateApplication::IsInitialized())
    {
        TSharedPtr<SWindow> W = FSlateApplication::Get().GetActiveTopLevelWindow();
        if (W.IsValid() && W->GetNativeWindow().IsValid())
            H = W->GetNativeWindow()->GetOSWindowHandle();
    }
    // стартовая папка — скриншоты Steam NMS
    FString StartDir = TEXT("C:/Program Files (x86)/Steam/userdata");
    TArray<FString> Picked;
    if (DP->OpenFileDialog(H, TEXT("Выбрать скриншот базы"), StartDir, TEXT(""),
        TEXT("Изображения (*.jpg;*.png)|*.jpg;*.png"), EFileDialogFlags::None, Picked)
        && Picked.Num() > 0)
    {
        SMBaseScreens.Add(BaseName, Picked[0]);
        SMScreenBrushes.Remove(BaseName); // сброс кэша
        SMSaveScreenLinks();
        SMRebuildBaseList();
    }
}

TSharedRef<SToolTip> SNMSBuilderUI::SMMakeBaseTooltip(const FNMSSaveBaseInfo& B)
{
    const FString Tip = FString::Printf(TEXT("Деталей: %d  |  Тип: %s  |  Адрес: %s"),
        B.PartCount, *B.BaseType, *B.GalacticAddress);
    const FSlateBrush* Screen = SMGetScreenBrush(B.BaseName);

    TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
    // фото (если привязано)
    if (Screen)
    {
        Box->AddSlot().AutoHeight().Padding(2)
        [
            SNew(SBox).WidthOverride(320).HeightOverride(180)
            [ SNew(SImage).Image(Screen) ]
        ];
    }
    // имя базы
    Box->AddSlot().AutoHeight().Padding(2)
    [ SNew(STextBlock).Text(FText::FromString(B.BaseName))
        .ColorAndOpacity(FSlateColor(FLinearColor(1.f,0.85f,0.3f))) ];
    // инфо
    Box->AddSlot().AutoHeight().Padding(2)
    [ SNew(STextBlock).Text(FText::FromString(Tip)) ];
    if (!Screen)
        Box->AddSlot().AutoHeight().Padding(2)
        [ SNew(STextBlock)
            .Text(FText::FromString(TEXT("(ПКМ — привязать скриншот)")))
            .ColorAndOpacity(FSlateColor(FLinearColor(0.55f,0.55f,0.55f))) ];

    return SNew(SToolTip)[ Box ];
}

// =================== ПРАВЫЕ ПАНЕЛИ: TRANSFORM выбранной ===================
#undef LOCTEXT_NAMESPACE
