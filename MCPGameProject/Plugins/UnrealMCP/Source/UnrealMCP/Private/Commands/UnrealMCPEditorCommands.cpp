#include "Commands/UnrealMCPEditorCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#include "ImageUtils.h"
#include "HighResScreenshot.h"
#include "Engine/GameViewportClient.h"
#include "Misc/FileHelper.h"
#include "GameFramework/Actor.h"
#include "Engine/Selection.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Camera/CameraActor.h"
#include "Components/StaticMeshComponent.h"
#include "EditorSubsystem.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "UnrealMCPModule.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"
#include "ILiveCodingModule.h"
#include "EngineUtils.h"

FUnrealMCPEditorCommands::FUnrealMCPEditorCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    // Actor manipulation commands
    if (CommandType == TEXT("get_actors_in_level"))
    {
        return HandleGetActorsInLevel(Params);
    }
    else if (CommandType == TEXT("find_actors_by_name"))
    {
        return HandleFindActorsByName(Params);
    }
    else if (CommandType == TEXT("spawn_actor") || CommandType == TEXT("create_actor"))
    {
        if (CommandType == TEXT("create_actor"))
        {
            UE_LOG(LogTemp, Warning, TEXT("'create_actor' command is deprecated and will be removed in a future version. Please use 'spawn_actor' instead."));
        }
        return HandleSpawnActor(Params);
    }
    else if (CommandType == TEXT("delete_actor"))
    {
        return HandleDeleteActor(Params);
    }
    else if (CommandType == TEXT("set_actor_transform"))
    {
        return HandleSetActorTransform(Params);
    }
    else if (CommandType == TEXT("get_actor_properties"))
    {
        return HandleGetActorProperties(Params);
    }
    else if (CommandType == TEXT("set_actor_property"))
    {
        return HandleSetActorProperty(Params);
    }
    // Blueprint actor spawning
    else if (CommandType == TEXT("spawn_blueprint_actor"))
    {
        return HandleSpawnBlueprintActor(Params);
    }
    // Editor viewport commands
    else if (CommandType == TEXT("focus_viewport"))
    {
        return HandleFocusViewport(Params);
    }
    else if (CommandType == TEXT("take_screenshot"))
    {
        return HandleTakeScreenshot(Params);
    }
    else if (CommandType == TEXT("trigger_live_coding"))
    {
        return HandleTriggerLiveCoding(Params);
    }
    else if (CommandType == TEXT("get_live_coding_status"))
    {
        return HandleGetLiveCodingStatus(Params);
    }
    else if (CommandType == TEXT("get_static_mesh_actors_with_bounds"))
    {
        return HandleGetStaticMeshActorsWithBounds(Params);
    }
    else if (CommandType == TEXT("bulk_spawn_nav_modifier_volumes"))
    {
        return HandleBulkSpawnNavModifierVolumes(Params);
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown editor command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetActorsInLevel(const TSharedPtr<FJsonObject>& Params)
{
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    TArray<TSharedPtr<FJsonValue>> ActorArray;
    for (AActor* Actor : AllActors)
    {
        if (Actor)
        {
            ActorArray.Add(FUnrealMCPCommonUtils::ActorToJson(Actor));
        }
    }
    
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("actors"), ActorArray);
    
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleFindActorsByName(const TSharedPtr<FJsonObject>& Params)
{
    FString Pattern;
    if (!Params->TryGetStringField(TEXT("pattern"), Pattern))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'pattern' parameter"));
    }
    
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    TArray<TSharedPtr<FJsonValue>> MatchingActors;
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName().Contains(Pattern))
        {
            MatchingActors.Add(FUnrealMCPCommonUtils::ActorToJson(Actor));
        }
    }
    
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("actors"), MatchingActors);
    
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSpawnActor(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString ActorType;
    if (!Params->TryGetStringField(TEXT("type"), ActorType))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'type' parameter"));
    }

    // Get actor name (required parameter)
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Get optional transform parameters
    FVector Location(0.0f, 0.0f, 0.0f);
    FRotator Rotation(0.0f, 0.0f, 0.0f);
    FVector Scale(1.0f, 1.0f, 1.0f);

    if (Params->HasField(TEXT("location")))
    {
        Location = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        Rotation = FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"));
    }
    if (Params->HasField(TEXT("scale")))
    {
        Scale = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale"));
    }

    // Create the actor based on type
    AActor* NewActor = nullptr;
    UWorld* World = GEditor->GetEditorWorldContext().World();

    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    // Check if an actor with this name already exists
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor with name '%s' already exists"), *ActorName));
        }
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *ActorName;

    if (ActorType == TEXT("StaticMeshActor"))
    {
        NewActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("PointLight"))
    {
        NewActor = World->SpawnActor<APointLight>(APointLight::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("SpotLight"))
    {
        NewActor = World->SpawnActor<ASpotLight>(ASpotLight::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("DirectionalLight"))
    {
        NewActor = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("CameraActor"))
    {
        NewActor = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("NavModifierVolume"))
    {
        // Look up at runtime so we don't link NavigationSystem statically.
        UClass* Cls = FindFirstObject<UClass>(TEXT("NavModifierVolume"), EFindFirstObjectOptions::ExactClass);
        if (Cls)
        {
            NewActor = World->SpawnActor<AActor>(Cls, Location, Rotation, SpawnParams);
        }
    }
    else
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown actor type: %s"), *ActorType));
    }

    if (NewActor)
    {
        // Set scale (since SpawnActor only takes location and rotation)
        FTransform Transform = NewActor->GetTransform();
        Transform.SetScale3D(Scale);
        NewActor->SetActorTransform(Transform);

        // Return the created actor's details
        return FUnrealMCPCommonUtils::ActorToJsonObject(NewActor, true);
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create actor"));
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleDeleteActor(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            // Store actor info before deletion for the response
            TSharedPtr<FJsonObject> ActorInfo = FUnrealMCPCommonUtils::ActorToJsonObject(Actor);
            
            // Delete the actor
            Actor->Destroy();
            
            TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
            ResultObj->SetObjectField(TEXT("deleted_actor"), ActorInfo);
            return ResultObj;
        }
    }
    
    return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSetActorTransform(const TSharedPtr<FJsonObject>& Params)
{
    // Get actor name
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Find the actor
    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }

    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Get transform parameters
    FTransform NewTransform = TargetActor->GetTransform();

    if (Params->HasField(TEXT("location")))
    {
        NewTransform.SetLocation(FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location")));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        NewTransform.SetRotation(FQuat(FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"))));
    }
    if (Params->HasField(TEXT("scale")))
    {
        NewTransform.SetScale3D(FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale")));
    }

    // Set the new transform
    TargetActor->SetActorTransform(NewTransform);

    // Return updated actor info
    return FUnrealMCPCommonUtils::ActorToJsonObject(TargetActor, true);
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetActorProperties(const TSharedPtr<FJsonObject>& Params)
{
    // Get actor name
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Find the actor
    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }

    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Always return detailed properties for this command
    return FUnrealMCPCommonUtils::ActorToJsonObject(TargetActor, true);
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSetActorProperty(const TSharedPtr<FJsonObject>& Params)
{
    // Get actor name
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Find the actor
    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }

    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Get property name
    FString PropertyName;
    if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'property_name' parameter"));
    }

    // Get property value
    if (!Params->HasField(TEXT("property_value")))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'property_value' parameter"));
    }
    
    TSharedPtr<FJsonValue> PropertyValue = Params->Values.FindRef(TEXT("property_value"));
    
    // Set the property using our utility function
    FString ErrorMessage;
    if (FUnrealMCPCommonUtils::SetObjectProperty(TargetActor, PropertyName, PropertyValue, ErrorMessage))
    {
        // Property set successfully
        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetStringField(TEXT("actor"), ActorName);
        ResultObj->SetStringField(TEXT("property"), PropertyName);
        ResultObj->SetBoolField(TEXT("success"), true);
        
        // Also include the full actor details
        ResultObj->SetObjectField(TEXT("actor_details"), FUnrealMCPCommonUtils::ActorToJsonObject(TargetActor, true));
        return ResultObj;
    }
    else
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSpawnBlueprintActor(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }

    // Find the blueprint
    if (BlueprintName.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Blueprint name is empty"));
    }

    FString Root      = TEXT("/Game/Blueprints/");
    FString AssetPath = Root + BlueprintName;

    if (!FPackageName::DoesPackageExist(AssetPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint '%s' not found – it must reside under /Game/Blueprints"), *BlueprintName));
    }

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Get transform parameters
    FVector Location(0.0f, 0.0f, 0.0f);
    FRotator Rotation(0.0f, 0.0f, 0.0f);
    FVector Scale(1.0f, 1.0f, 1.0f);

    if (Params->HasField(TEXT("location")))
    {
        Location = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        Rotation = FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"));
    }
    if (Params->HasField(TEXT("scale")))
    {
        Scale = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale"));
    }

    // Spawn the actor
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    FTransform SpawnTransform;
    SpawnTransform.SetLocation(Location);
    SpawnTransform.SetRotation(FQuat(Rotation));
    SpawnTransform.SetScale3D(Scale);

    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *ActorName;

    AActor* NewActor = World->SpawnActor<AActor>(Blueprint->GeneratedClass, SpawnTransform, SpawnParams);
    if (NewActor)
    {
        return FUnrealMCPCommonUtils::ActorToJsonObject(NewActor, true);
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to spawn blueprint actor"));
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleFocusViewport(const TSharedPtr<FJsonObject>& Params)
{
    // Get target actor name if provided
    FString TargetActorName;
    bool HasTargetActor = Params->TryGetStringField(TEXT("target"), TargetActorName);

    // Get location if provided
    FVector Location(0.0f, 0.0f, 0.0f);
    bool HasLocation = false;
    if (Params->HasField(TEXT("location")))
    {
        Location = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
        HasLocation = true;
    }

    // Get distance
    float Distance = 1000.0f;
    if (Params->HasField(TEXT("distance")))
    {
        Distance = Params->GetNumberField(TEXT("distance"));
    }

    // Get orientation if provided
    FRotator Orientation(0.0f, 0.0f, 0.0f);
    bool HasOrientation = false;
    if (Params->HasField(TEXT("orientation")))
    {
        Orientation = FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("orientation"));
        HasOrientation = true;
    }

    // Get the active viewport
    FLevelEditorViewportClient* ViewportClient = (FLevelEditorViewportClient*)GEditor->GetActiveViewport()->GetClient();
    if (!ViewportClient)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get active viewport"));
    }

    // If we have a target actor, focus on it
    if (HasTargetActor)
    {
        // Find the actor
        AActor* TargetActor = nullptr;
        TArray<AActor*> AllActors;
        UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
        
        for (AActor* Actor : AllActors)
        {
            if (Actor && Actor->GetName() == TargetActorName)
            {
                TargetActor = Actor;
                break;
            }
        }

        if (!TargetActor)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *TargetActorName));
        }

        // Focus on the actor
        ViewportClient->SetViewLocation(TargetActor->GetActorLocation() - FVector(Distance, 0.0f, 0.0f));
    }
    // Otherwise use the provided location
    else if (HasLocation)
    {
        ViewportClient->SetViewLocation(Location - FVector(Distance, 0.0f, 0.0f));
    }
    else
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Either 'target' or 'location' must be provided"));
    }

    // Set orientation if provided
    if (HasOrientation)
    {
        ViewportClient->SetViewRotation(Orientation);
    }

    // Force viewport to redraw
    ViewportClient->Invalidate();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleTakeScreenshot(const TSharedPtr<FJsonObject>& Params)
{
    // Get file path parameter
    FString FilePath;
    if (!Params->TryGetStringField(TEXT("filepath"), FilePath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'filepath' parameter"));
    }
    
    // Ensure the file path has a proper extension
    if (!FilePath.EndsWith(TEXT(".png")))
    {
        FilePath += TEXT(".png");
    }

    // Get the active viewport
    if (GEditor && GEditor->GetActiveViewport())
    {
        FViewport* Viewport = GEditor->GetActiveViewport();
        TArray<FColor> Bitmap;
        FIntRect ViewportRect(0, 0, Viewport->GetSizeXY().X, Viewport->GetSizeXY().Y);
        
        if (Viewport->ReadPixels(Bitmap, FReadSurfaceDataFlags(), ViewportRect))
        {
            TArray<uint8> CompressedBitmap;
            FImageUtils::CompressImageArray(Viewport->GetSizeXY().X, Viewport->GetSizeXY().Y, Bitmap, CompressedBitmap);
            
            if (FFileHelper::SaveArrayToFile(CompressedBitmap, *FilePath))
            {
                TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
                ResultObj->SetStringField(TEXT("filepath"), FilePath);
                return ResultObj;
            }
        }
    }
    
    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to take screenshot"));
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetLiveCodingStatus(const TSharedPtr<FJsonObject>& Params)
{
    const FLiveCodingState State = FUnrealMCPModule::GetLiveCodingState();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("is_compiling"), State.bCompiling);
    Result->SetBoolField(TEXT("has_result"), State.bHasResult);

    if (State.bHasResult)
    {
        const double NowSeconds = FPlatformTime::Seconds();
        const double Age = FMath::Max(0.0, NowSeconds - State.LastFinishTimeSeconds);
        Result->SetNumberField(TEXT("seconds_since_last_finish"), Age);
    }
    Result->SetBoolField(TEXT("success"), true);
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleTriggerLiveCoding(const TSharedPtr<FJsonObject>& Params)
{
    // Use the LiveCoding module directly so we can verify the compile actually
    // kicked off. GEngine->Exec was unreliable — returned true for the
    // recognized console-command name regardless of whether LC honored it.

    ILiveCodingModule* LC = FModuleManager::GetModulePtr<ILiveCodingModule>("LiveCoding");
    if (!LC)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("LiveCoding module not loaded"));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();

    if (LC->IsCompiling())
    {
        ResultObj->SetBoolField(TEXT("success"), true);
        ResultObj->SetBoolField(TEXT("already_compiling"), true);
        ResultObj->SetBoolField(TEXT("is_compiling_now"), true);
        ResultObj->SetStringField(TEXT("message"), TEXT("Live Coding already compiling — request ignored."));
        return ResultObj;
    }

    if (!LC->IsEnabledForSession())
    {
        LC->EnableForSession(true);
    }
    if (!LC->IsEnabledByDefault())
    {
        // Best-effort; not all UE versions expose a setter, but enabling for
        // the session is the important bit.
    }

    UE_LOG(LogTemp, Display, TEXT("UnrealMCP: trigger_live_coding invoked, calling LC->Compile()"));
    LC->Compile();

    // Give LC's worker pipeline a brief moment to flip IsCompiling=true. If it
    // doesn't, the caller knows the trigger didn't take. (probe)
    FPlatformProcess::Sleep(0.15f);
    const bool bIsCompilingNow = LC->IsCompiling();

    ResultObj->SetBoolField(TEXT("success"), bIsCompilingNow);
    ResultObj->SetBoolField(TEXT("already_compiling"), false);
    ResultObj->SetBoolField(TEXT("is_compiling_now"), bIsCompilingNow);
    ResultObj->SetStringField(TEXT("message"), bIsCompilingNow
        ? TEXT("Live Coding compile started. Poll get_live_coding_status or call wait_for_live_coding.")
        : TEXT("LC->Compile() called but IsCompiling did not flip — likely no code changes detected, or LC disabled."));
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetStaticMeshActorsWithBounds(const TSharedPtr<FJsonObject>& Params)
{
    // Optional substring filter on mesh asset path (case-insensitive).
    FString MeshFilter;
    Params->TryGetStringField(TEXT("mesh_filter"), MeshFilter);
    const bool bHasFilter = !MeshFilter.IsEmpty();

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : GWorld;
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world available"));
    }

    TArray<TSharedPtr<FJsonValue>> EntryArray;

    for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
    {
        AStaticMeshActor* Actor = *It;
        if (!Actor)
        {
            continue;
        }

        UStaticMeshComponent* SMC = Actor->GetStaticMeshComponent();
        if (!SMC)
        {
            continue;
        }

        UStaticMesh* Mesh = SMC->GetStaticMesh();
        const FString MeshPath = Mesh ? Mesh->GetPathName() : FString();

        if (bHasFilter && MeshPath.Find(MeshFilter, ESearchCase::IgnoreCase) == INDEX_NONE)
        {
            continue;
        }

        // World-space bounds for the static mesh component (includes actor transform).
        const FBoxSphereBounds Bounds = SMC->Bounds;
        const FVector Origin = Bounds.Origin;
        const FVector Extent = Bounds.BoxExtent;
        const FVector BoxMin = Origin - Extent;
        const FVector BoxMax = Origin + Extent;

        TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("name"), Actor->GetName());
        Entry->SetStringField(TEXT("mesh_path"), MeshPath);

        auto WriteVec = [](TSharedPtr<FJsonObject>& Parent, const FString& Key, const FVector& V)
        {
            TArray<TSharedPtr<FJsonValue>> Arr;
            Arr.Add(MakeShared<FJsonValueNumber>(V.X));
            Arr.Add(MakeShared<FJsonValueNumber>(V.Y));
            Arr.Add(MakeShared<FJsonValueNumber>(V.Z));
            Parent->SetArrayField(Key, Arr);
        };

        WriteVec(Entry, TEXT("location"), Actor->GetActorLocation());
        WriteVec(Entry, TEXT("box_min"), BoxMin);
        WriteVec(Entry, TEXT("box_max"), BoxMax);
        WriteVec(Entry, TEXT("extent"), Extent);

        EntryArray.Add(MakeShared<FJsonValueObject>(Entry));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("actors"), EntryArray);
    Result->SetNumberField(TEXT("count"), EntryArray.Num());
    Result->SetBoolField(TEXT("success"), true);
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleBulkSpawnNavModifierVolumes(const TSharedPtr<FJsonObject>& Params)
{
    // Expected params:
    //   volumes: [{ center:[X,Y,Z], size:[X,Y,Z], name?: "..." }, ...]
    //   area_class: "/Script/NavigationSystem.NavArea_Null" (default if absent)
    //   name_prefix: "NavBlock_" (default)
    const TArray<TSharedPtr<FJsonValue>>* VolumesPtr = nullptr;
    if (!Params->TryGetArrayField(TEXT("volumes"), VolumesPtr) || !VolumesPtr)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'volumes' array"));
    }

    FString AreaClassPath = TEXT("/Script/NavigationSystem.NavArea_Null");
    Params->TryGetStringField(TEXT("area_class"), AreaClassPath);

    FString NamePrefix = TEXT("NavBlock_");
    Params->TryGetStringField(TEXT("name_prefix"), NamePrefix);

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : GWorld;
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world available"));
    }

    // Resolve NavModifierVolume class once.
    UClass* VolumeCls = FindFirstObject<UClass>(TEXT("NavModifierVolume"), EFindFirstObjectOptions::ExactClass);
    if (!VolumeCls)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("ANavModifierVolume class not loaded; ensure NavigationSystem module is initialized"));
    }

    // Resolve area class.
    UClass* AreaCls = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), nullptr, *AreaClassPath));
    if (!AreaCls)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Could not load area class: %s"), *AreaClassPath));
    }

    // The AreaClass UPROPERTY on ANavModifierVolume.
    FProperty* AreaClassProp = VolumeCls->FindPropertyByName(TEXT("AreaClass"));

    int32 Spawned = 0;
    TArray<TSharedPtr<FJsonValue>> SpawnedNames;

    auto ReadVec = [](const TSharedPtr<FJsonObject>& Obj, const FString& Key, FVector& Out) -> bool
    {
        const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
        if (!Obj->TryGetArrayField(Key, Arr) || !Arr || Arr->Num() < 3) return false;
        Out.X = (*Arr)[0]->AsNumber();
        Out.Y = (*Arr)[1]->AsNumber();
        Out.Z = (*Arr)[2]->AsNumber();
        return true;
    };

    for (int32 Idx = 0; Idx < VolumesPtr->Num(); ++Idx)
    {
        const TSharedPtr<FJsonObject> Entry = (*VolumesPtr)[Idx]->AsObject();
        if (!Entry.IsValid()) continue;

        FVector Center, Size;
        if (!ReadVec(Entry, TEXT("center"), Center)) continue;
        if (!ReadVec(Entry, TEXT("size"), Size)) continue;

        FString InstanceName;
        if (!Entry->TryGetStringField(TEXT("name"), InstanceName))
        {
            InstanceName = FString::Printf(TEXT("%s%03d"), *NamePrefix, Idx);
        }

        FActorSpawnParameters SpawnParams;
        SpawnParams.Name = *InstanceName;
        // Avoid name-collision spin: if exists, append index suffix.
        if (StaticFindObjectFast(nullptr, World->PersistentLevel, SpawnParams.Name) != nullptr)
        {
            SpawnParams.Name = *FString::Printf(TEXT("%s_AutoName"), *InstanceName);
        }

        AActor* NewActor = World->SpawnActor<AActor>(VolumeCls, Center, FRotator::ZeroRotator, SpawnParams);
        if (!NewActor) continue;

        // Default NavModifierVolume brush is roughly 200x200x200 (full-extent).
        // Scale so the cube covers the supplied size on each axis.
        FVector Scale(
            FMath::Max(0.01f, Size.X / 200.f),
            FMath::Max(0.01f, Size.Y / 200.f),
            FMath::Max(0.01f, Size.Z / 200.f));
        NewActor->SetActorScale3D(Scale);

        // Assign AreaClass via reflection. AreaClass is a UClass* (TSubclassOf<UNavArea>).
        if (AreaClassProp)
        {
            if (FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(AreaClassProp))
            {
                void* PropAddr = ObjProp->ContainerPtrToValuePtr<void>(NewActor);
                ObjProp->SetObjectPropertyValue(PropAddr, AreaCls);
            }
        }

        // Push the change so the nav system rebuilds.
        NewActor->PostEditChange();
        NewActor->MarkPackageDirty();

        ++Spawned;
        SpawnedNames.Add(MakeShared<FJsonValueString>(NewActor->GetName()));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("spawned"), Spawned);
    Result->SetNumberField(TEXT("requested"), VolumesPtr->Num());
    Result->SetArrayField(TEXT("names"), SpawnedNames);
    Result->SetBoolField(TEXT("success"), Spawned > 0);
    return Result;
} 