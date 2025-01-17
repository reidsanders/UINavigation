// Copyright (C) 2023 Gonçalo Marques - All Rights Reserved

#include "UINavComponent.h"
#include "UINavWidget.h"
#include "UINavPCComponent.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Framework/Application/SlateApplication.h"
#include "Internationalization/Internationalization.h"
#include "Sound/SoundBase.h"
#include "UINavMacros.h"
#include "UINavSettings.h"
#include "UINavPCReceiver.h"
#include "Slate/SObjectWidget.h"
#include "Templates/SharedPointer.h"
#include "UINavigationConfig.h"

UUINavComponent::UUINavComponent(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	SetIsFocusable(true);

	ComponentText = FText::FromString(TEXT("Button Text"));
}

void UUINavComponent::NativeConstruct()
{
	NavButton->OnClicked.AddUniqueDynamic(this, &UUINavComponent::OnButtonClicked);
	NavButton->OnPressed.AddUniqueDynamic(this, &UUINavComponent::OnButtonPressed);
	NavButton->OnReleased.AddUniqueDynamic(this, &UUINavComponent::OnButtonReleased);
	NavButton->OnHovered.AddUniqueDynamic(this, &UUINavComponent::OnButtonHovered);
	NavButton->OnUnhovered.AddUniqueDynamic(this, &UUINavComponent::OnButtonUnhovered);

	Super::NativeConstruct();

	if (!IsValid(ParentWidget))
	{
		ParentWidget = UUINavWidget::GetOuterObject<UUINavWidget>(this);

		if (!IsValid(ParentWidget))
		{
			DISPLAYERROR("UI Nav Component isn't in a UINavWidget!");
		}
		else if (!IsValid(ParentWidget->GetFirstComponent()) && CanBeNavigated())
		{
			ParentWidget->SetFirstComponent(this);
			if (ParentWidget->bCompletedSetup)
			{
				SetFocus();
			}
		}
	}
}

void UUINavComponent::NativeDestruct()
{
	if (IsValid(ParentWidget) && !ParentWidget->IsBeingRemoved())
	{
		ParentWidget->RemovedComponent(this);
	}
	Super::NativeDestruct();
}

bool UUINavComponent::Initialize()
{
	return Super::Initialize();
}

FReply UUINavComponent::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	FReply Reply = Super::NativeOnKeyDown(InGeometry, InKeyEvent);
	
	if (!IsValid(ParentWidget))
	{
		return Reply;
	}

	if (FSlateApplication::Get().GetNavigationActionFromKey(InKeyEvent) == EUINavigationAction::Accept)
	{
		if (!ParentWidget->TryConsumeNavigation())
		{
			ParentWidget->StartedSelect();
		}
	}

	TSharedRef<FUINavigationConfig> NavConfig = StaticCastSharedRef<FUINavigationConfig>(FSlateApplication::Get().GetNavigationConfig());
	EUINavigation Direction = NavConfig->GetNavigationDirectionFromKey(InKeyEvent);
	if (Direction == EUINavigation::Invalid)
	{
		Direction = NavConfig->GetNavigationDirectionFromAnalogKey(InKeyEvent);
	}
	if (Direction != EUINavigation::Invalid)
	{
		ParentWidget->UINavPC->NotifyNavigationKeyPressed(InKeyEvent.GetKey(), Direction);
	}

	return Reply;
}

FReply UUINavComponent::NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	FReply Reply = Super::NativeOnKeyUp(InGeometry, InKeyEvent);

	if (!IsValid(ParentWidget) || !IsValid(ParentWidget->UINavPC))
	{
		return Reply;
	}

	if (FSlateApplication::Get().GetNavigationActionFromKey(InKeyEvent) == EUINavigationAction::Accept)
	{
		if (!ParentWidget->TryConsumeNavigation())
		{
			ParentWidget->StoppedSelect();
		}
	}
	else if (FSlateApplication::Get().GetNavigationActionFromKey(InKeyEvent) == EUINavigationAction::Back)
	{
		if (!ParentWidget->TryConsumeNavigation())
		{
			ParentWidget->StoppedReturn();
			IUINavPCReceiver::Execute_OnReturn(ParentWidget->UINavPC->GetOwner());
		}
	}
	else
	{
		TSharedRef<FUINavigationConfig> NavConfig = StaticCastSharedRef<FUINavigationConfig>(FSlateApplication::Get().GetNavigationConfig());
		EUINavigation Direction = NavConfig->GetNavigationDirectionFromKey(InKeyEvent);
		if (Direction == EUINavigation::Invalid)
		{
			Direction = NavConfig->GetNavigationDirectionFromAnalogKey(InKeyEvent);
		}
		if (Direction != EUINavigation::Invalid)
		{
			ParentWidget->UINavPC->NotifyNavigationKeyReleased(InKeyEvent.GetKey(), Direction);
		}
	}

	return Reply;
}

FReply UUINavComponent::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	FReply Reply = Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);

	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton &&
		IsValid(ParentWidget) &&
		IsValid(ParentWidget->UINavPC) &&
		ParentWidget->UINavPC->IsListeningToInputRebind())
	{
		ParentWidget->UINavPC->ProcessRebind(EKeys::LeftMouseButton);
	}

	return Reply;
}

FReply UUINavComponent::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	FReply Reply = Super::NativeOnMouseButtonDoubleClick(InGeometry, InMouseEvent);

	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton &&
		IsValid(ParentWidget) &&
		IsValid(ParentWidget->UINavPC) &&
		ParentWidget->UINavPC->IsListeningToInputRebind())
	{
		ParentWidget->UINavPC->ProcessRebind(EKeys::LeftMouseButton);
	}

	return Reply;
}

void UUINavComponent::OnNavigatedTo_Implementation()
{
}

void UUINavComponent::OnNavigatedFrom_Implementation()
{
}

void UUINavComponent::HandleFocusReceived()
{
	if (!CanBeNavigated())
	{
		return;
	}

	if (IsValid(ParentWidget))
	{
		ParentWidget->NavigatedTo(this);
	}
}

void UUINavComponent::HandleFocusLost()
{
}

void UUINavComponent::OnButtonClicked()
{
	OnNativeClicked.Broadcast();
	OnClicked.Broadcast();

	ExecuteComponentActions(EComponentAction::OnClicked);

	if (!IsValid(ParentWidget) || !IsValid(ParentWidget->UINavPC))
	{
		return;
	}
	
	IUINavPCReceiver::Execute_OnSelect(ParentWidget->UINavPC->GetOwner());
}

void UUINavComponent::OnButtonPressed()
{
	OnNativePressed.Broadcast();
	OnPressed.Broadcast();

	if (IsValid(ParentWidget))
	{
		ParentWidget->OnPressedComponent(this);
	}

	ExecuteComponentActions(EComponentAction::OnPressed);
}

void UUINavComponent::OnButtonReleased()
{
	OnNativeReleased.Broadcast();
	OnReleased.Broadcast();

	if (IsValid(ParentWidget))
	{
		ParentWidget->OnReleasedComponent(this);
	}

	ExecuteComponentActions(EComponentAction::OnReleased);
}

void UUINavComponent::OnButtonHovered()
{
	if (IsValid(ParentWidget))
	{
		ParentWidget->OnHoveredComponent(this);
	}
}

void UUINavComponent::OnButtonUnhovered()
{
	if (IsValid(ParentWidget))
	{
		ParentWidget->OnUnhoveredComponent(this);
	}
}

void UUINavComponent::SetText(const FText& Text)
{
	ComponentText = Text;

	if (IsValid(NavText))
	{
		NavText->SetText(ComponentText);
	}
}

void UUINavComponent::SwitchTextColorTo(FLinearColor Color)
{
	if (IsValid(NavText) && bUseTextColor)
	{
		NavText->SetColorAndOpacity(Color);
	}
}

void UUINavComponent::SwitchTextColorToDefault()
{
	SwitchTextColorTo(TextDefaultColor);
}

void UUINavComponent::SwitchTextColorToNavigated()
{
	SwitchTextColorTo(TextNavigatedColor);
}

void UUINavComponent::ExecuteComponentActions(const EComponentAction Action)
{
	const FComponentActions* const ActionObjects = ComponentActions.Find(Action);
	if (ActionObjects == nullptr)
	{
		return;
	}

	for (const UUINavComponentAction* const ActionObject : ActionObjects->Actions)
	{
		if (!IsValid(ActionObject))
		{
			continue;
		}

		UUINavComponentAction* DuplicatedAction = DuplicateObject<UUINavComponentAction>(ActionObject, ActionObject->GetOuter());
		if (!IsValid(DuplicatedAction))
		{
			continue;
		}

		DuplicatedAction->ExecuteAction(this);
	}
}

bool UUINavComponent::CanBeNavigated() const
{
	const bool bIgnoreDisabled = GetDefault<UUINavSettings>()->bIgnoreDisabledButton;
	return ((GetVisibility() == ESlateVisibility::Visible || GetVisibility() == ESlateVisibility::SelfHitTestInvisible) &&
		(GetIsEnabled() || !bIgnoreDisabled) &&
		NavButton->GetVisibility() == ESlateVisibility::Visible &&
		(NavButton->GetIsEnabled() || !bIgnoreDisabled));
}

FReply UUINavComponent::NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent)
{
	FReply Reply = Super::NativeOnFocusReceived(InGeometry, InFocusEvent);

	HandleFocusReceived();
	NavButton->SetFocus();

	return Reply;
}

void UUINavComponent::NativeOnFocusLost(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnFocusLost(InFocusEvent);

	HandleFocusLost();
}

void UUINavComponent::NativeOnFocusChanging(const FWeakWidgetPath& PreviousFocusPath, const FWidgetPath& NewWidgetPath, const FFocusEvent& InFocusEvent)
{
	Super::NativeOnFocusChanging(PreviousFocusPath, NewWidgetPath, InFocusEvent);

	const bool bHadFocus = PreviousFocusPath.ContainsWidget(&TakeWidget().Get());
	const bool bHasFocus = NewWidgetPath.ContainsWidget(&TakeWidget().Get());
	const bool bHasButtonFocus = NewWidgetPath.ContainsWidget(&NavButton->TakeWidget().Get());

	if (NewWidgetPath.Widgets.Num() == 0)
	{
		return;
	}

	if (InFocusEvent.GetCause() == EFocusCause::Navigation && ParentWidget->UINavPC->IgnoreFocusByNavigation())
	{
		int WidgetIndex = PreviousFocusPath.Widgets.Num() - 1;
		TSharedPtr<SWidget> PreviousWidget = PreviousFocusPath.Widgets[WidgetIndex].Pin();
		if (PreviousWidget.IsValid())
		{
			while (!PreviousWidget->GetType().IsEqual(FName(TEXT("SObjectWidget"))) && --WidgetIndex > 0)
			{
				PreviousWidget = PreviousFocusPath.Widgets[WidgetIndex].Pin();
				TSharedPtr<SObjectWidget> PreviousUserWidget = StaticCastSharedPtr<SObjectWidget>(PreviousWidget);
				PreviousUserWidget->GetWidgetObject()->SetFocus();
			}
			return;
		}
	}

	const FString WidgetTypeString = NewWidgetPath.GetLastWidget()->GetType().ToString();
	if (!WidgetTypeString.Contains(TEXT("SObjectWidget")) &&
		!WidgetTypeString.Contains(TEXT("SButton")) &&
		!WidgetTypeString.Contains(TEXT("SSpinBox")) &&
		!WidgetTypeString.Contains(TEXT("SEditableText")))
	{
		NavButton->SetFocus();
		return;
	}

	if (!bHadFocus && bHasFocus)
	{
		HandleFocusReceived();
	}
	else if (bHadFocus && !bHasFocus)
	{
		HandleFocusLost();
	}

	if (bHasFocus && !bHasButtonFocus)
	{
		NavButton->SetFocus();
	}
}

FNavigationReply UUINavComponent::NativeOnNavigation(const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent, const FNavigationReply& InDefaultReply)
{
	FNavigationReply Reply = Super::NativeOnNavigation(MyGeometry, InNavigationEvent, InDefaultReply);

	if (!IsValid(ParentWidget) || !IsValid(ParentWidget->UINavPC))
	{
		return Reply;
	}

	if (!ParentWidget->UINavPC->AllowsNavigatingDirection(InNavigationEvent.GetNavigationType()))
	{
		return FNavigationReply::Stop();
	}

	if (ParentWidget->TryConsumeNavigation())
	{
		return FNavigationReply::Stop();
	}

	if (!ParentWidget->UINavPC->TryNavigateInDirection(InNavigationEvent.GetNavigationType(), InNavigationEvent.GetNavigationGenesis()))
	{
		return FNavigationReply::Stop();
	}

	const bool bStopNextPrevious = GetDefault<UUINavSettings>()->bStopNextPreviousNavigation;
	const bool bAllowsSectionInput = ParentWidget->UINavPC->AllowsSectionInput();

	if (InNavigationEvent.GetNavigationType() == EUINavigation::Next)
	{
		if (bAllowsSectionInput)
		{
			ParentWidget->PropagateOnNext();

			IUINavPCReceiver::Execute_OnNext(ParentWidget->UINavPC->GetOwner());
		}

		if (bStopNextPrevious || !bAllowsSectionInput)
		{
			ParentWidget->UINavPC->SetIgnoreFocusByNavigation(true);
			return FNavigationReply::Stop();
		}
	}
		
	if (InNavigationEvent.GetNavigationType() == EUINavigation::Previous)
	{
		if (bAllowsSectionInput)
		{
			IUINavPCReceiver::Execute_OnPrevious(ParentWidget->UINavPC->GetOwner());
			ParentWidget->PropagateOnPrevious();
		}

		if (bStopNextPrevious || !bAllowsSectionInput)
		{
			ParentWidget->UINavPC->SetIgnoreFocusByNavigation(true);
			return FNavigationReply::Stop();
		}
	}
	else if (InNavigationEvent.GetNavigationType() != EUINavigation::Invalid &&
			InNavigationEvent.GetNavigationType() != EUINavigation::Next &&
			InNavigationEvent.GetNavigationType() != EUINavigation::Previous)
	{
		IUINavPCReceiver::Execute_OnNavigated(ParentWidget->UINavPC->GetOwner(), InNavigationEvent.GetNavigationType());
	}

	return Reply;
}

void UUINavComponent::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (IsValid(NavText))
	{
		NavText->SetText(ComponentText);

		if (bOverride_Font)
		{
			NavText->SetFont(FontOverride);
		}
		else
		{
			FontOverride = NavText->GetFont();
		}

		if (bUseTextColor)
		{
			NavText->SetColorAndOpacity(TextDefaultColor);
		}
	}
	
	if (IsValid(NavButton))
	{
		if (bOverride_Style)
		{
			NavButton->SetStyle(StyleOverride);
		}
		else
		{
			StyleOverride = NavButton->GetStyle();
		}
	}
}

void UUINavComponent::SwitchButtonStyle(const EButtonStyle NewStyle, const bool bRevertStyle /*= true*/)
{
	if (NewStyle == ForcedStyle)
	{
		return;
	}

	CurrentStyle = GetStyleFromButtonState();
	if (NewStyle == CurrentStyle && ForcedStyle == EButtonStyle::None) return;

	const bool bWasForcePressed = ForcedStyle == EButtonStyle::Pressed;

	if (bRevertStyle)
	{
		RevertButtonStyle();
	}

	SwapStyle(NewStyle, CurrentStyle);

	if (NewStyle != CurrentStyle)
	{
		ForcedStyle = NewStyle;
	}
}

void UUINavComponent::RevertButtonStyle()
{
	if (ForcedStyle == EButtonStyle::None) return;

	SwapStyle(ForcedStyle, CurrentStyle);

	ForcedStyle = EButtonStyle::None;
}

void UUINavComponent::SwapStyle(EButtonStyle Style1, EButtonStyle Style2)
{
	FButtonStyle Style = NavButton->GetStyle();
	FSlateBrush TempState;

	switch (Style1)
	{
	case EButtonStyle::Normal:
		TempState = Style.Normal;
		switch (Style2)
		{
		case EButtonStyle::Hovered:
			Style.Normal = Style.Hovered;
			Style.Hovered = TempState;
			break;
		case EButtonStyle::Pressed:
			Style.Normal = Style.Pressed;
			Style.Pressed = TempState;
			break;
		}
		break;
	case EButtonStyle::Hovered:
		TempState = Style.Hovered;
		switch (Style2)
		{
		case EButtonStyle::Normal:
			Style.Hovered = Style.Normal;
			Style.Normal = TempState;
			break;
		case EButtonStyle::Pressed:
			Style.Hovered = Style.Pressed;
			Style.Pressed = TempState;
			break;
		}
		break;
	case EButtonStyle::Pressed:
		TempState = Style.Pressed;
		switch (Style2)
		{
		case EButtonStyle::Normal:
			Style.Pressed = Style.Normal;
			Style.Normal = TempState;
			break;
		case EButtonStyle::Hovered:
			Style.Pressed = Style.Hovered;
			Style.Hovered = TempState;
			break;
		}
		break;
	}

	NavButton->SetStyle(Style);
}

EButtonStyle UUINavComponent::GetStyleFromButtonState()
{
	if (NavButton->IsPressed()) return EButtonStyle::Pressed;
	else if (NavButton->IsHovered()) return EButtonStyle::Hovered;
	else return EButtonStyle::Normal;
}
