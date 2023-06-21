// Copyright (C) 2023 Gonçalo Marques - All Rights Reserved

#include "UINavWidget.h"
#include "UINavHorizontalComponent.h"
#include "UINavComponent.h"
#include "UINavInputBox.h"
#include "UINavPCComponent.h"
#include "UINavPCReceiver.h"
#include "UINavPromptWidget.h"
#include "UINavSettings.h"
#include "UINavWidgetComponent.h"
#include "UINavBlueprintFunctionLibrary.h"
#include "UINavMacros.h"
#include "ComponentActions/UINavComponentAction.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/HorizontalBox.h"
#include "Components/VerticalBox.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ActorComponent.h"
#include "Components/ListView.h"
#include "Engine/GameViewportClient.h"
#include "Engine/ViewportSplitScreen.h"
#include "Curves/CurveFloat.h"
#if IS_VR_PLATFORM
#include "HeadMountedDisplayFunctionLibrary.h"
#endif

UUINavWidget::UUINavWidget(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UUINavWidget::NativeConstruct()
{
	bBeingRemoved = false;

	const UWorld* const World = GetWorld();
	OuterUINavWidget = GetOuterObject<UUINavWidget>(this);
	if (OuterUINavWidget != nullptr)
	{
		ParentWidget = OuterUINavWidget;
		PreSetup(!bCompletedSetup);

		ConfigureUINavPC();

		Super::NativeConstruct();
		return;
	}
	if (World != nullptr)
	{
		if (const UGameViewportClient* ViewportClient = World->GetGameViewport())
		{
			bUsingSplitScreen = ViewportClient->GetCurrentSplitscreenConfiguration() != ESplitScreenType::None;
		}
	}
	/*
	If this widget was added through a parent widget and should remove it from the viewport,
	remove that widget from viewport
	*/
	if (ParentWidget != nullptr && ParentWidget->IsInViewport() && bParentRemoved)
	{
		UUINavWidget* OuterParentWidget = ParentWidget->GetMostOuterUINavWidget();
		OuterParentWidget->bReturningToParent = true;
		OuterParentWidget->RemoveFromParent();

		if (bShouldDestroyParent)
		{
			OuterParentWidget->Destruct();
			OuterParentWidget = nullptr;
		}
	}

	//If this widget was added through a child widget, destroy it
	if (ReturnedFromWidget != nullptr)
	{
		APlayerController* PC = Cast<APlayerController>(UINavPC->GetOwner());
		if (ReturnedFromWidget->HasUserFocus(PC))
		{
			SetUserFocus(PC);
			if (UINavPC->GetInputMode() == EInputMode::UI)
			{
				SetKeyboardFocus();
			}
		}

		if (WidgetComp == nullptr) ReturnedFromWidget->Destruct();
	}

	PreSetup(!bCompletedSetup);
	InitialSetup();

	Super::NativeConstruct();
}

void UUINavWidget::InitialSetup(const bool bRebuilding)
{
	if (!bRebuilding)
	{
		WidgetClass = GetClass();
		if (UINavPC == nullptr)
		{
			ConfigureUINavPC();
		}

		//If widget was already setup, apply only certain steps
		if (bCompletedSetup)
		{
			ReconfigureSetup();
			return;
		}

		bSetupStarted = true;
	}

	TraverseHierarchy();

	//If this widget doesn't need to create the selector, skip to setup
	if (!IsSelectorValid())
	{
		UINavSetup();
	}
	else
	{
		SetupSelector();
		UINavSetupWaitForTick = 0;
	}
}

void UUINavWidget::ReconfigureSetup()
{
	bSetupStarted = true;

	if (!IsSelectorValid())
	{
		UINavSetup();
	}
	else
	{
		SetupSelector();
		UINavSetupWaitForTick = 0;
	}

	for (UUINavWidget* ChildUINavWidget : ChildUINavWidgets)
	{
		ChildUINavWidget->ReconfigureSetup();
	}
}

void UUINavWidget::CleanSetup()
{
	for (UUINavWidget* ChildUINavWidget : ChildUINavWidgets)
	{
		ChildUINavWidget->CleanSetup();
	}
	
	bSetupStarted = false;
}

void UUINavWidget::ConfigureUINavPC()
{
	APlayerController* PC = Cast<APlayerController>(GetOwningPlayer());
	if (PC == nullptr)
	{
		DISPLAYERROR("Player Controller is Null!");
		return;
	}
	UINavPC = Cast<UUINavPCComponent>(PC->GetComponentByClass(UUINavPCComponent::StaticClass()));
	if (UINavPC == nullptr)
	{
		DISPLAYERROR("Player Controller doesn't have a UINavPCComponent!");
		return;
	}
}

void UUINavWidget::TraverseHierarchy()
{
	//Find UINavButtons in the widget hierarchy
	TArray<UWidget*> Widgets;
	WidgetTree->GetAllWidgets(Widgets);
	for (UWidget* Widget : Widgets)
	{
		UUINavWidget* ChildUINavWidget = Cast<UUINavWidget>(Widget);
		if (ChildUINavWidget != nullptr)
		{
			ChildUINavWidget->AddParentToPath(ChildUINavWidgets.Num());
			ChildUINavWidgets.Add(ChildUINavWidget);
		}
	}
}

void UUINavWidget::SetupSelector()
{
	TheSelector->SetVisibility(ESlateVisibility::Hidden);

	UCanvasPanelSlot* SelectorSlot = Cast<UCanvasPanelSlot>(TheSelector->Slot);

	SelectorSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	SelectorSlot->SetPosition(FVector2D(0.f, 0.f));
}

void UUINavWidget::UINavSetup()
{
	if (UINavPC == nullptr) return;

	bCompletedSetup = true;

	if (ReturnedFromWidget != nullptr && IsValid(CurrentComponent))
	{
		CurrentComponent->SetFocus();
		if (!GetDefault<UUINavSettings>()->bForceNavigation && !IsValid(HoveredComponent))
		{
			UnforceNavigation();
		}
	}
	else
	{
		if (!TryFocusOnInitialComponent())
		{
			UINavPC->NotifyNavigatedTo(this);
		}
	}

	ReturnedFromWidget = nullptr;

	IgnoreHoverComponent = nullptr;

	OnSetupCompleted();
}

UUINavComponent* UUINavWidget::GetInitialFocusComponent_Implementation()
{
	return FirstComponent;
}

bool UUINavWidget::TryFocusOnInitialComponent()
{
	UUINavComponent* InitialComponent = GetInitialFocusComponent();
	if (IsValid(InitialComponent))
	{
		InitialComponent->SetFocus();
		if (!GetDefault<UUINavSettings>()->bForceNavigation && !IsValid(HoveredComponent))
		{
			UnforceNavigation();
		}
		return true;
	}

	return false;
}

void UUINavWidget::PropagateGainNavigation(UUINavWidget* PreviousActiveWidget, UUINavWidget* NewActiveWidget, const UUINavWidget* const CommonParent)
{
	if (this == PreviousActiveWidget) return;

	if (OuterUINavWidget != nullptr && this != CommonParent)
	{
		OuterUINavWidget->PropagateGainNavigation(PreviousActiveWidget, NewActiveWidget, CommonParent);
	}

	if (this != CommonParent || this == NewActiveWidget)
	{
		GainNavigation(PreviousActiveWidget);
	}
}

void UUINavWidget::GainNavigation(UUINavWidget* PreviousActiveWidget)
{
	if (bHasNavigation) return;

	if (IsValid(FirstComponent))
	{
		bHasNavigation = true;

		if (IsSelectorValid())
		{
			TheSelector->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}

	const bool bPreviousWidgetIsChild = PreviousActiveWidget != nullptr ?
                                    UUINavBlueprintFunctionLibrary::ContainsArray<int>(PreviousActiveWidget->GetUINavWidgetPath(), UINavWidgetPath) :
                                    false;
	OnGainedNavigation(PreviousActiveWidget, bPreviousWidgetIsChild);
}

void UUINavWidget::OnGainedNavigation_Implementation(UUINavWidget* PreviousActiveWidget, const bool bFromChild)
{
}

void UUINavWidget::PropagateLoseNavigation(UUINavWidget* NewActiveWidget, UUINavWidget* PreviousActiveWidget, const UUINavWidget* const CommonParent)
{
	if (this == NewActiveWidget) return;

	if (this != CommonParent || this == PreviousActiveWidget)
	{
		LoseNavigation(NewActiveWidget);
	}

	if (OuterUINavWidget != nullptr && this != CommonParent)
	{
		OuterUINavWidget->PropagateLoseNavigation(NewActiveWidget, PreviousActiveWidget, CommonParent);
	}
}

void UUINavWidget::LoseNavigation(UUINavWidget* NewActiveWidget)
{
	if (!bHasNavigation) return;

	const bool bHaveSameOuter = NewActiveWidget->GetMostOuterUINavWidget() == GetMostOuterUINavWidget();

	const bool bNewWidgetIsChild = NewActiveWidget != nullptr && NewActiveWidget->GetUINavWidgetPath().Num() > 0 ?
		UUINavBlueprintFunctionLibrary::ContainsArray<int>(NewActiveWidget->GetUINavWidgetPath(), UINavWidgetPath) && bHaveSameOuter :
		false;

	if (bNewWidgetIsChild && !bMaintainNavigationForChild)
	{
		return;
	}

	if (NewActiveWidget == nullptr ||
		(!bNewWidgetIsChild && NewActiveWidget->bMaintainNavigationForChild))
	{
		UpdateNavigationVisuals(nullptr, true);
	}

	CallOnNavigate(CurrentComponent, nullptr);

	bHasNavigation = false;

	if (!bNewWidgetIsChild && bHaveSameOuter) CurrentComponent = nullptr;

	OnLostNavigation(NewActiveWidget, bNewWidgetIsChild);
}

void UUINavWidget::OnLostNavigation_Implementation(UUINavWidget* NewActiveWidget, const bool bToChild)
{
}

void UUINavWidget::SetCurrentComponent(UUINavComponent* Component)
{
	CurrentComponent = Component;

	if (IsValid(OuterUINavWidget) && !OuterUINavWidget->bMaintainNavigationForChild)
	{
		OuterUINavWidget->SetCurrentComponent(Component);
	}
}

void UUINavWidget::SetHoveredComponent(UUINavComponent* Component)
{
	HoveredComponent = Component;

	if (IsValid(OuterUINavWidget) && !OuterUINavWidget->bMaintainNavigationForChild)
	{
		OuterUINavWidget->SetHoveredComponent(Component);
	}
}

void UUINavWidget::SetSelectedComponent(UUINavComponent* Component)
{
	SelectedComponent = Component;

	if (IsValid(OuterUINavWidget) && !OuterUINavWidget->bMaintainNavigationForChild)
	{
		OuterUINavWidget->SetSelectedComponent(Component);
	}
}

FReply UUINavWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	FReply Reply = Super::NativeOnKeyDown(InGeometry, InKeyEvent);

	if (!IsValid(CurrentComponent))
	{
		if (FSlateApplication::Get().GetNavigationActionFromKey(InKeyEvent) == EUINavigationAction::Accept)
		{
			if (!TryConsumeNavigation())
			{
				StartedSelect();
			}
		}
	}

	return Reply;
}

FReply UUINavWidget::NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	FReply Reply = Super::NativeOnKeyUp(InGeometry, InKeyEvent);

	if (!IsValid(CurrentComponent))
	{
		if (FSlateApplication::Get().GetNavigationActionFromKey(InKeyEvent) == EUINavigationAction::Accept)
		{
			if (!TryConsumeNavigation())
			{
				StoppedSelect();
			}
		}
		else if (FSlateApplication::Get().GetNavigationActionFromKey(InKeyEvent) == EUINavigationAction::Back)
		{
			if (!TryConsumeNavigation())
			{
				StoppedReturn();
			}
		}
	}

	return Reply;
}

void UUINavWidget::NativeTick(const FGeometry & MyGeometry, float DeltaTime)
{
	Super::NativeTick(MyGeometry, DeltaTime);

	if (IsSelectorValid())
	{
		if (UINavSetupWaitForTick >= 0)
		{
			if (UINavSetupWaitForTick >= 1)
			{
				UINavSetup();
				UINavSetupWaitForTick = -1;
			}
			else
			{
				UINavSetupWaitForTick++;
			}
		}

		if (UpdateSelectorWaitForTick >= 0)
		{
			if (UpdateSelectorWaitForTick >= 1)
			{
				if (MoveCurve != nullptr) BeginSelectorMovement(UpdateSelectorPrevComponent, UpdateSelectorNextComponent);
				else UpdateSelectorLocation(UpdateSelectorNextComponent);
				UpdateSelectorWaitForTick = -1;
			}
			else
			{
				UpdateSelectorWaitForTick++;
			}
		}

		if (bMovingSelector)
		{
			HandleSelectorMovement(DeltaTime);
		}
	}
}

void UUINavWidget::RemoveFromParent()
{
	bBeingRemoved = true;
	if (OuterUINavWidget == nullptr && !bReturningToParent && !bDestroying && !GetFName().IsNone() && IsValid(this) &&
	    (ParentWidget != nullptr || (bAllowRemoveIfRoot && UINavPC != nullptr)))
	{
		ReturnToParent();
		return;
	}
	bReturningToParent = false;

	Super::RemoveFromParent();
}

FReply UUINavWidget::NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent)
{
	FReply Reply = Super::NativeOnFocusReceived(InGeometry, InFocusEvent);

	if (OuterUINavWidget != nullptr)
	{
		if (IsValid(CurrentComponent))
		{
			CurrentComponent->SetFocus();
		}
		else
		{
			TryFocusOnInitialComponent();
		}

		return Reply;
	}

	if (IsValid(CurrentComponent))
	{
		CurrentComponent->SetFocus();
		return Reply;
	}

	TryFocusOnInitialComponent();

	return Reply;
}

void UUINavWidget::NativeOnFocusLost(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnFocusLost(InFocusEvent);
}

void UUINavWidget::HandleSelectorMovement(const float DeltaTime)
{
	if (MoveCurve == nullptr) return;

	MovementCounter += DeltaTime;

	if (MovementCounter >= MovementTime)
	{
		MovementCounter = 0.f;
		bMovingSelector = false;
		TheSelector->SetRenderTranslation(SelectorDestination);
		return;
	}

	TheSelector->SetRenderTranslation(SelectorOrigin + Distance*MoveCurve->GetFloatValue(MovementCounter));
}

void UUINavWidget::UpdateSelectorLocation(UUINavComponent* Component)
{
	if (TheSelector == nullptr || !IsValid(FirstComponent)) return;
	TheSelector->SetRenderTranslation(GetButtonLocation(Component));
}

FVector2D UUINavWidget::GetButtonLocation(UUINavComponent* Component) const
{
	if (!IsValid(Component))
	{
		return FVector2D();
	}

	const FGeometry Geom = Component->NavButton->GetCachedGeometry();
	const FVector2D LocalSize = Geom.GetLocalSize();
	FVector2D LocalPosition;
	switch (SelectorPositioning)
	{
		case ESelectorPosition::Position_Center:
			LocalPosition = LocalSize / 2;
			break;
		case ESelectorPosition::Position_Top:
			LocalPosition = FVector2D(LocalSize.X / 2, 0.f);
			break;
		case ESelectorPosition::Position_Bottom:
			LocalPosition = FVector2D(LocalSize.X / 2, LocalSize.Y);
			break;
		case ESelectorPosition::Position_Left:
			LocalPosition = FVector2D(0.f, LocalSize.Y / 2);
			break;
		case ESelectorPosition::Position_Right:
			LocalPosition = FVector2D(LocalSize.X, LocalSize.Y / 2);
			break;
		case ESelectorPosition::Position_Top_Right:
			LocalPosition = FVector2D(LocalSize.X, 0.f);
			break;
		case ESelectorPosition::Position_Top_Left:
			LocalPosition = FVector2D(0.f, 0.f);
			break;
		case ESelectorPosition::Position_Bottom_Right:
			LocalPosition = FVector2D(LocalSize.X, LocalSize.Y);
			break;
		case ESelectorPosition::Position_Bottom_Left:
			LocalPosition = FVector2D(0.f, LocalSize.Y);
			break;
	}
	
	FVector2D PixelPos, ViewportPos;
	USlateBlueprintLibrary::LocalToViewport(GetWorld(), Geom, LocalPosition, PixelPos, ViewportPos);
	ViewportPos += SelectorOffset;
	return ViewportPos;
}

void UUINavWidget::ExecuteAnimations(UUINavComponent* FromComponent, UUINavComponent* ToComponent, const bool bHadNavigation)
{
	if (IsValid(FromComponent) &&
		FromComponent != ToComponent &&
		IsValid(FromComponent->GetComponentAnimation()) &&
		FromComponent->UseComponentAnimation() &&
		bHadNavigation &&
		(bForcingNavigation || !IsValid(ToComponent)))
	{
		if (FromComponent->IsAnimationPlaying(FromComponent->GetComponentAnimation()))
		{
			FromComponent->ReverseAnimation(FromComponent->GetComponentAnimation());
		}
		else
		{
			FromComponent->PlayAnimation(FromComponent->GetComponentAnimation(), 0.0f, 1, EUMGSequencePlayMode::Reverse);
		}
	}

	if (IsValid(ToComponent) &&
		IsValid(ToComponent->GetComponentAnimation()) &&
		ToComponent->UseComponentAnimation())
	{
		if (ToComponent->IsAnimationPlaying(ToComponent->GetComponentAnimation()))
		{
			ToComponent->ReverseAnimation(ToComponent->GetComponentAnimation());
		}
		else
		{
			ToComponent->PlayAnimation(ToComponent->GetComponentAnimation(), 0.0f, 1, EUMGSequencePlayMode::Forward);
		}
	}
}

void UUINavWidget::UpdateButtonStates(UUINavComponent* Component)
{
	if (IsValid(CurrentComponent))
	{
		CurrentComponent->SwitchButtonStyle(EButtonStyle::Normal);
	}
	if (IsValid(Component))
	{
		Component->SwitchButtonStyle(EButtonStyle::Hovered);
	}
}

void UUINavWidget::UpdateTextColor(UUINavComponent* Component)
{
	if (IsValid(CurrentComponent))
	{
		CurrentComponent->SwitchTextColorToDefault();
	}
	if (IsValid(Component))
	{
		Component->SwitchTextColorToNavigated();
	}
}

void UUINavWidget::SetSelectorScale(FVector2D NewScale)
{
	if (TheSelector == nullptr) return;
	TheSelector->SetRenderScale(NewScale);
}

void UUINavWidget::SetSelectorVisibility(const bool bVisible)
{
	if (TheSelector == nullptr) return;
	const ESlateVisibility Vis = bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden;
	TheSelector->SetVisibility(Vis);
}

bool UUINavWidget::IsSelectorVisible()
{
	if (TheSelector == nullptr) return false;
	return TheSelector->GetVisibility() == ESlateVisibility::HitTestInvisible;
}

void UUINavWidget::OnNavigate_Implementation(UUINavComponent* FromComponent, UUINavComponent* TomComponent)
{

}

void UUINavWidget::OnSelect_Implementation(UUINavComponent* Component)
{

}

void UUINavWidget::PropagateOnSelect(UUINavComponent* Component)
{
	OnSelect(Component);
	if (IsValid(OuterUINavWidget) && !OuterUINavWidget->bMaintainNavigationForChild)
	{
		OuterUINavWidget->PropagateOnSelect(Component);
	}
}

void UUINavWidget::OnStartSelect_Implementation(UUINavComponent* Component)
{

}

void UUINavWidget::PropagateOnStartSelect(UUINavComponent* Component)
{
	OnStartSelect(Component);
	if (IsValid(OuterUINavWidget) && !OuterUINavWidget->bMaintainNavigationForChild)
	{
		OuterUINavWidget->PropagateOnStartSelect(Component);
	}
}

void UUINavWidget::OnStopSelect_Implementation(UUINavComponent* Component)
{

}

void UUINavWidget::PropagateOnStopSelect(UUINavComponent* Component)
{
	OnStopSelect(Component);
	if (IsValid(OuterUINavWidget) && !OuterUINavWidget->bMaintainNavigationForChild)
	{
		OuterUINavWidget->PropagateOnStopSelect(Component);
	}
}

void UUINavWidget::UpdateNavigationVisuals(UUINavComponent* Component, const bool bHadNavigation, const bool bBypassForcedNavigation /*= false*/)
{
	if (IsValid(Component) && IsSelectorValid())
	{
		UpdateSelectorPrevComponent = CurrentComponent;
		UpdateSelectorNextComponent = Component;
		UpdateSelectorWaitForTick = 0;
	}

	UpdateTextColor(Component);

	UpdateButtonStates(Component);

	ExecuteAnimations(CurrentComponent, Component, bHadNavigation);
}

void UUINavWidget::BeginSelectorMovement(UUINavComponent* FromComponent, UUINavComponent* ToComponent)
{
	if (MoveCurve == nullptr) return;

	SelectorOrigin = (bMovingSelector || !IsValid(FromComponent)) ? TheSelector->GetRenderTransform().Translation : GetButtonLocation(FromComponent);
	SelectorDestination = GetButtonLocation(ToComponent);
	Distance = SelectorDestination - SelectorOrigin;

	float MinTime, MaxTime;
	MoveCurve->GetTimeRange(MinTime, MaxTime);
	MovementTime = MaxTime - MinTime;
	MovementCounter = 0.0f;

	bMovingSelector = true;
}

void UUINavWidget::AttemptUnforceNavigation(const EInputType NewInputType)
{
	if (!GetDefault<UUINavSettings>()->bForceNavigation && NewInputType == EInputType::Mouse)
	{
		if (IsValid(HoveredComponent))
		{
			if (HoveredComponent != CurrentComponent)
			{
				HoveredComponent->SetFocus();
			}
		}
		else
		{
			UnforceNavigation();
		}
	}
}

void UUINavWidget::ForceNavigation()
{
	bForcingNavigation = true;
	UpdateNavigationVisuals(CurrentComponent, true);
	if (IsValid(CurrentComponent))
	{
		CurrentComponent->SwitchButtonStyle(EButtonStyle::Hovered);
	}
}

void UUINavWidget::UnforceNavigation()
{
	bForcingNavigation = false;
	UpdateNavigationVisuals(nullptr, true);
	if (IsValid(CurrentComponent))
	{
		CurrentComponent->RevertButtonStyle();
	}
}

void UUINavWidget::OnReturn_Implementation()
{
	if(GetDefault<UUINavSettings>()->bRemoveWidgetOnReturn) ReturnToParent();
}

void UUINavWidget::OnNext_Implementation()
{

}

void UUINavWidget::PropagateOnNext()
{
	OnNext();
	if (IsValid(OuterUINavWidget) && !OuterUINavWidget->bMaintainNavigationForChild)
	{
		OuterUINavWidget->PropagateOnNext();
	}
}

void UUINavWidget::OnPrevious_Implementation()
{

}

void UUINavWidget::PropagateOnPrevious()
{
	OnPrevious();
	if (IsValid(OuterUINavWidget) && !OuterUINavWidget->bMaintainNavigationForChild)
	{
		OuterUINavWidget->PropagateOnPrevious();
	}
}

void UUINavWidget::OnInputChanged_Implementation(const EInputType From, const EInputType To)
{

}

void UUINavWidget::PreSetup_Implementation(const bool bFirstSetup)
{

}

void UUINavWidget::OnSetupCompleted_Implementation()
{

}

void UUINavWidget::OnHorizCompNavigateLeft_Implementation(UUINavComponent* Component)
{

}

void UUINavWidget::OnHorizCompNavigateRight_Implementation(UUINavComponent* Component)
{

}

void UUINavWidget::OnHorizCompUpdated_Implementation(UUINavComponent* Component)
{

}

UUINavWidget* UUINavWidget::GoToWidget(TSubclassOf<UUINavWidget> NewWidgetClass, const bool bRemoveParent, const bool bDestroyParent, const int ZOrder)
{
	if (NewWidgetClass == nullptr)
	{
		DISPLAYERROR("GoToWidget: No Widget Class found");
		return nullptr;
	}

	APlayerController* PC = Cast<APlayerController>(UINavPC->GetOwner());
	UUINavWidget* NewWidget = CreateWidget<UUINavWidget>(PC, NewWidgetClass);
	return GoToBuiltWidget(NewWidget, bRemoveParent, bDestroyParent, ZOrder);
}

UUINavWidget* UUINavWidget::GoToPromptWidget(TSubclassOf<UUINavPromptWidget> NewWidgetClass, const FPromptWidgetDecided& Event, const FText Title, const FText Message, const bool bRemoveParent /*= false*/, const int ZOrder /*= 0*/)
{
	if (NewWidgetClass == nullptr)
	{
		DISPLAYERROR("GoToPromptWidget: No Widget Class found");
		return nullptr;
	}

	if (!Event.IsBound())
	{
		DISPLAYERROR("GoToPromptWidget: Event isn't bound");
		return nullptr;
	}

	APlayerController* PC = Cast<APlayerController>(UINavPC->GetOwner());
	UUINavPromptWidget* NewWidget = CreateWidget<UUINavPromptWidget>(PC, NewWidgetClass);
	NewWidget->Title = Title;
	NewWidget->Message = Message;
	NewWidget->SetCallback(Event);
	return GoToBuiltWidget(NewWidget, bRemoveParent, false, ZOrder);
}

UUINavWidget * UUINavWidget::GoToBuiltWidget(UUINavWidget* NewWidget, const bool bRemoveParent, const bool bDestroyParent, const int ZOrder)
{
	if (NewWidget == nullptr) return nullptr;

	UUINavWidget* OldOuterUINavWidget = GetMostOuterUINavWidget();
	UUINavWidget* NewOuterUINavWidget = NewWidget->GetMostOuterUINavWidget();

	if (IsValid(HoveredComponent))
	{
		IgnoreHoverComponent = HoveredComponent;
	}
	
	if (OuterUINavWidget != nullptr || NewOuterUINavWidget == this)
	{
		if (NewOuterUINavWidget == OldOuterUINavWidget)
		{
			return NewWidget;
		}
	}
	
	NewWidget->ParentWidget = GetMostOuterUINavWidget();
	NewWidget->bParentRemoved = bRemoveParent;
	NewWidget->bShouldDestroyParent = bDestroyParent;
	NewWidget->WidgetComp = WidgetComp;
	if (WidgetComp != nullptr)
	{
		WidgetComp->SetWidget(NewWidget);
	}
	else
	{
		if (!bForceUsePlayerScreen && (!bUsingSplitScreen || NewWidget->bUseFullscreenWhenSplitscreen)) NewWidget->AddToViewport(ZOrder);
		else NewWidget->AddToPlayerScreen(ZOrder);

		APlayerController* PC = Cast<APlayerController>(UINavPC->GetOwner());
		NewWidget->SetUserFocus(PC);
		if (UINavPC->GetInputMode() == EInputMode::UI)
		{
			NewWidget->SetKeyboardFocus();
		}
	}
	OldOuterUINavWidget->CleanSetup();
	
	return NewWidget;
}

void UUINavWidget::ReturnToParent(const bool bRemoveAllParents, const int ZOrder)
{
	if (ParentWidget == nullptr)
	{
		if (bAllowRemoveIfRoot && UINavPC != nullptr)
		{
			UINavPC->SetActiveWidget(nullptr);

			SelectCount = 0;
			SetSelectedComponent(nullptr);
			if (WidgetComp != nullptr)
			{
				WidgetComp->SetWidget(nullptr);
			}
			else
			{
				bReturningToParent = true;
				RemoveFromParent();
			}
		}
		return;
	}

	SelectCount = 0;
	SetSelectedComponent(nullptr);
	if (WidgetComp != nullptr)
	{
		if (bRemoveAllParents)
		{
			WidgetComp->SetWidget(nullptr);
		}
		else
		{
			if (bParentRemoved)
			{
				ParentWidget->ReturnedFromWidget = this;
			}
			else
			{
				ParentWidget->ReconfigureSetup();
			}
			WidgetComp->SetWidget(ParentWidget);

		}
	}
	else
	{
		if (OuterUINavWidget == nullptr)
		{
			if (bRemoveAllParents)
			{
				IUINavPCReceiver::Execute_OnRootWidgetRemoved(UINavPC->GetOwner());
				UINavPC->SetActiveWidget(nullptr);
				ParentWidget->RemoveAllParents();
				bReturningToParent = true;
				RemoveFromParent();
				Destruct();
			}
			else
			{
				//If parent was removed, add it to viewport
				if (bParentRemoved)
				{
					if (IsValid(ParentWidget))
					{
						ParentWidget->ReturnedFromWidget = this;
						if (!bForceUsePlayerScreen && (!bUsingSplitScreen || ParentWidget->bUseFullscreenWhenSplitscreen)) ParentWidget->AddToViewport(ZOrder);
						else ParentWidget->AddToPlayerScreen(ZOrder);
					}
				}
				else
				{
					UUINavWidget* ParentOuter = ParentWidget->GetMostOuterUINavWidget();
					ParentWidget->ReturnedFromWidget = this;
					ParentWidget->ReconfigureSetup();
				}
				bReturningToParent = true;
				RemoveFromParent();
			}
		}
		else
		{
			OuterUINavWidget->ReturnToParent();
		}
	}
}

void UUINavWidget::RemoveAllParents()
{
	if (ParentWidget != nullptr)
	{
		ParentWidget->RemoveAllParents();
	}
	bReturningToParent = true;
	RemoveFromParent();
	Destruct();
}

int UUINavWidget::GetWidgetHierarchyDepth(UWidget* Widget) const
{
	if (Widget == nullptr) return -1;

	int DepthCount = 0;
	UPanelWidget* Parent = Widget->GetParent();
	while (Parent != nullptr)
	{
		DepthCount++;
		Parent = Parent->GetParent();
	}
	return DepthCount;
}

void UUINavWidget::NavigatedTo(UUINavComponent* NavigatedToComponent, const bool bNotifyUINavPC /*= true*/)
{
	if (!IsValid(UINavPC) ||
		(CurrentComponent == NavigatedToComponent && UINavPC->GetActiveSubWidget() == this))
	{
		return;
	}

	const bool bHadNavigation = bHasNavigation;

	if (bNotifyUINavPC)
	{
		UINavPC->NotifyNavigatedTo(this);
	}

	if (IsValid(OuterUINavWidget) && !OuterUINavWidget->bMaintainNavigationForChild)
	{
		OnNavigate(CurrentComponent, NavigatedToComponent);

		CurrentComponent = NavigatedToComponent;

		OuterUINavWidget->NavigatedTo(NavigatedToComponent, false);
		return;
	}

	if (CurrentComponent != NavigatedToComponent && (CurrentComponent != nullptr || bForcingNavigation))
	{
		UpdateNavigationVisuals(NavigatedToComponent, !bHoverRestoredNavigation);
	}

	CallOnNavigate(bHadNavigation == bHasNavigation ? CurrentComponent : nullptr, NavigatedToComponent);

	SetCurrentComponent(NavigatedToComponent);

	if (bHoverRestoredNavigation)
	{
		bHoverRestoredNavigation = false;
	}
}

void UUINavWidget::CallOnNavigate(UUINavComponent* FromComponent, UUINavComponent* ToComponent)
{
	OnNavigate(FromComponent, ToComponent);

	if (IsValid(FromComponent))
	{
		FromComponent->OnNavigatedFrom();
		FromComponent->ExecuteComponentActions(EComponentAction::OnNavigatedFrom);
	}

	if (IsValid(ToComponent))
	{
		ToComponent->OnNavigatedTo();
		ToComponent->ExecuteComponentActions(EComponentAction::OnNavigatedTo);
	}
}

void UUINavWidget::StartedSelect()
{
	PropagateOnStartSelect(CurrentComponent);
}

void UUINavWidget::StoppedSelect()
{
	if (SelectedComponent == CurrentComponent)
	{
		PropagateOnSelect(CurrentComponent);
	}
	PropagateOnStopSelect(CurrentComponent);
}

void UUINavWidget::StartedReturn()
{
}

void UUINavWidget::StoppedReturn()
{
	if (IsValid(UINavPC))
	{
		OnReturn();
	}
}

bool UUINavWidget::TryConsumeNavigation()
{
	if (!GetDefault<UUINavSettings>()->bForceNavigation && !bForcingNavigation)
	{
		ForceNavigation();
		return true;
	}

	return IsValid(SelectedComponent);
}

UUINavWidget* UUINavWidget::GetMostOuterUINavWidget()
{
	UUINavWidget* MostOUter = this;
	while (MostOUter->OuterUINavWidget != nullptr)
	{
		MostOUter = MostOUter->OuterUINavWidget;
	}

	return MostOUter;
}

UUINavWidget* UUINavWidget::GetChildUINavWidget(const int ChildIndex) const
{
	return ChildIndex < ChildUINavWidgets.Num() ? ChildUINavWidgets[ChildIndex] : nullptr;
}

void UUINavWidget::AddParentToPath(const int IndexInParent)
{
	UINavWidgetPath.EmplaceAt(0, IndexInParent);

	for (UUINavWidget* ChildUINavWidget : ChildUINavWidgets)
	{
		ChildUINavWidget->AddParentToPath(IndexInParent);
	}
}

void UUINavWidget::SetFirstComponent(UUINavComponent* Component)
{
	if (IsValid(FirstComponent))
	{
		return;
	}

	FirstComponent = Component;

	if (IsValid(OuterUINavWidget) && !IsValid(OuterUINavWidget->GetFirstComponent()))
	{
		OuterUINavWidget->SetFirstComponent(Component);
	}
}

void UUINavWidget::RemovedComponent(UUINavComponent* Component)
{
	if (IsValid(Component) && Component == CurrentComponent)
	{
		SetCurrentComponent(nullptr);
	}
	
	if (IsValid(FirstComponent) && Component == FirstComponent)
	{
		FirstComponent = nullptr;
	}
}

bool UUINavWidget::IsSelectorValid()
{
	return TheSelector != nullptr && TheSelector->GetIsEnabled();
}

void UUINavWidget::OnHoveredComponent(UUINavComponent* Component)
{
	if (!IsValid(Component) || UINavPC == nullptr) return;

	UINavPC->CancelRebind();

	SetHoveredComponent(Component);

	if (Component == CurrentComponent && UINavPC->GetActiveSubWidget() == this)
	{
		Component->RevertButtonStyle();
	}

	if (!bForcingNavigation)
	{
		bForcingNavigation = true;
		bHoverRestoredNavigation = true;
		if (Component == CurrentComponent)
		{
			UpdateNavigationVisuals(CurrentComponent, false, true);
		}
	}

	if (Component != CurrentComponent || UINavPC->GetActiveSubWidget() != this)
	{
		Component->SetFocus();
	}
}

void UUINavWidget::OnUnhoveredComponent(UUINavComponent* Component)
{
	if (!IsValid(Component)) return;

	UINavPC->CancelRebind();

	if (IgnoreHoverComponent == nullptr || IgnoreHoverComponent != Component)
	{
		SetHoveredComponent(nullptr);
	}

	if (!GetDefault<UUINavSettings>()->bForceNavigation)
	{
		UnforceNavigation();
	}
	else
	{
		if (SelectedComponent != Component)
		{
			Component->SwitchButtonStyle(Component == CurrentComponent ? EButtonStyle::Hovered : EButtonStyle::Normal);
		}

		if (CurrentComponent == Component && (IgnoreHoverComponent == nullptr || IgnoreHoverComponent != Component))
		{
			Component->SetFocus();
		}
		else
		{
			Component->RevertButtonStyle();
		}
	}
}

void UUINavWidget::OnPressedComponent(UUINavComponent* Component)
{
	if (!IsValid(Component) || UINavPC == nullptr) return;

	if (!UINavPC->AllowsSelectInput()) return;

	SetSelectedComponent(Component);

	SelectCount++;

	PropagateOnStartSelect(CurrentComponent);
}

void UUINavWidget::OnReleasedComponent(UUINavComponent* Component)
{
	if (!IsValid(Component) || (!bHasNavigation && SelectCount == 0) || UINavPC == nullptr) return;

	if (!Component->NavButton->IsHovered()) Component->RevertButtonStyle();

	if (CurrentComponent == nullptr || SelectedComponent == nullptr || !IsValid(Component)) return;

	const bool bIsSelectedButton = SelectedComponent == Component;

	if (IsValid(Component))
	{
		Component->SwitchButtonStyle(Component->NavButton->IsPressed() || SelectCount > 1 ? EButtonStyle::Pressed : (Component == CurrentComponent ? EButtonStyle::Hovered : EButtonStyle::Normal));

		if (SelectCount > 0) SelectCount--;
		if (SelectCount == 0)
		{
			SetSelectedComponent(nullptr);

			if (bIsSelectedButton)
			{
				PropagateOnSelect(Component);
			}
			PropagateOnStopSelect(Component);
		}
	}

	if (Component != CurrentComponent) Component->SetFocus();
}
