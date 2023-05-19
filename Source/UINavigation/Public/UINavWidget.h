// Copyright (C) 2019 Gonçalo Marques - All Rights Reserved

#pragma once

#include "Blueprint/UserWidget.h"
#include "Data/InputType.h"
#include "Data/ReceiveInputType.h"
#include "Data/SelectorPosition.h"
#include "Data/NavigationEvent.h"
#include "Delegates/DelegateCombinations.h"
#include "UINavWidget.generated.h"

class UUINavComponent;
class UUINavHorizontalComponent;
class UUINavPromptWidget;
enum class EButtonStyle : uint8;

DECLARE_DYNAMIC_DELEGATE_OneParam(FPromptWidgetDecided, const UPromptDataBase*, PromptData);


/**
* This class contains the logic for UserWidget navigation
*/
UCLASS()
class UINAVIGATION_API UUINavWidget : public UUserWidget
{
	GENERATED_BODY()

protected:

	bool bMovingSelector = false;
	bool bBeingRemoved = false;
		
	UPROPERTY()
	UUINavComponent* UpdateSelectorPrevComponent = nullptr;
	UPROPERTY()
	UUINavComponent* UpdateSelectorNextComponent = nullptr;
	
	int8 UINavSetupWaitForTick = -1;
	int8 UpdateSelectorWaitForTick = -1;

	bool bReturningToParent = false;

	bool bDestroying = false;
	bool bHasNavigation = false;
	bool bForcingNavigation = true;
	bool bRestoreNavigation = false;

	UPROPERTY(BlueprintReadOnly, Category = UINavWidget)
	UUINavComponent* FirstComponent = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "UINavWidget")
	UUINavComponent* CurrentComponent = nullptr;

	UPROPERTY()
	UUINavComponent* HoveredComponent = nullptr;

	UPROPERTY()
	UUINavComponent* SelectedComponent = nullptr;

	uint8 SelectCount = 0;

	float MovementCounter;
	float MovementTime;

	FVector2D SelectorOrigin;
	FVector2D SelectorDestination;
	FVector2D Distance;


	TArray<int> UINavWidgetPath;

	//This widget's class
	TSubclassOf<UUINavWidget> WidgetClass;

	bool bUsingSplitScreen = false;

	/******************************************************************************/

	UUINavWidget(const FObjectInitializer& ObjectInitializer);

	/**
	*	Configures the blueprint on Construct event
	*/
	void InitialSetup(const bool bRebuilding = false);

	/**
	*	Resets the necessary variables in order for this widget to be used again
	*/
	void CleanSetup();

	/**
	*	Configures the UINavPC
	*/
	UFUNCTION(BlueprintCallable, Category = UINavWidget)
	void ConfigureUINavPC();

	/**
	*	Configures the selector on event Construct
	*/
	void SetupSelector();

	void SetEnableUINavButtons(const bool bEnable, const bool bRecursive);

	/**
	*	Returns the position of the UINavButton with the specified index
	*/
	FVector2D GetButtonLocation(UUINavComponent* Component) const;

	void BeginSelectorMovement(UUINavComponent* FromComponent, UUINavComponent* ToComponent);
	void HandleSelectorMovement(const float DeltaTime);

public:

	EReceiveInputType ReceiveInputType = EReceiveInputType::None;

	//The UserWidget object that will move along the Widget
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, OptionalWidget = true), Category = UINavWidget)
	UUserWidget* TheSelector = nullptr;

	//All the child UINavWidgets in this Widget
	UPROPERTY(BlueprintReadOnly, Category = UINavWidget)
	TArray<UUINavWidget*> ChildUINavWidgets;

	//Reference to the parent widget that created this widget
	UPROPERTY(BlueprintReadOnly, Category = UINavWidget)
	UUINavWidget* ParentWidget = nullptr;

	//Reference to the widget that encapsulates this widget
	UPROPERTY(BlueprintReadOnly, Category = UINavWidget)
	UUINavWidget* OuterUINavWidget = nullptr;

	//Current player controller
	UPROPERTY(BlueprintReadOnly, Category = UINavWidget)
	class UUINavPCComponent* UINavPC = nullptr;

	//Widget that created this widget (if returned from a child)
	UPROPERTY(BlueprintReadOnly, Category = UINavWidget)
	UUINavWidget* ReturnedFromWidget = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = UINavWidget)
	class UUINavWidgetComponent* WidgetComp = nullptr;

	//Should this widget remove its parent from the viewport when created?
	UPROPERTY(BlueprintReadOnly, Category = UINavWidget)
	bool bParentRemoved = false;

	//Should this widget destroy its parent
	UPROPERTY(BlueprintReadOnly, Category = UINavWidget)
	bool bShouldDestroyParent = false;
	
	/*If set to true, the UINavWidget will maintain its navigated state when navigation moves to a child nested widget,
	 otherwise, the button being navigated to at that moment will be navigated out of */
	UPROPERTY(EditDefaultsOnly, Category = UINavWidget)
	bool bMaintainNavigationForChild = false;

	/*If set to true, the gamepad's left thumbstick will be used to move the mouse */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = UINavWidget)
	bool bUseLeftThumbstickAsMouse = false;

	//If set to true, this widget will be removed if it has no ParentWidget and is returned from
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = UINavWidget)
	bool bAllowRemoveIfRoot = true;

    /*If set to true, the widget will be set to fullscreen even when using split screen */
    UPROPERTY(EditDefaultsOnly, Category = UINavWidget)
	bool bUseFullscreenWhenSplitscreen = false;

	// If set to true, will always use AddToPlayerScreen instead of AddToViewport, even if not in split screen
	UPROPERTY(EditDefaultsOnly, Category = UINavWidget)
	bool bForceUsePlayerScreen = false;

	bool bCompletedSetup = false;
	bool bSetupStarted = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "UINavigation Selector")
	UCurveFloat* MoveCurve = nullptr;

	//The position the selector will be in relative to the button
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "UINavigation Selector")
	ESelectorPosition SelectorPositioning = ESelectorPosition::Position_Center;

	//The offset to apply when positioning the selector on a button
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "UINavigation Selector")
	FVector2D SelectorOffset;


	/*********************************************************************************/

	
	virtual void NativeConstruct() override;

	virtual void NativeTick(const FGeometry & MyGeometry, float DeltaTime) override;

	virtual void RemoveFromParent() override;
	virtual void OnLevelRemovedFromWorld(ULevel* InLevel, UWorld* InWorld) override;

	virtual FReply NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnFocusLost(const FFocusEvent& InFocusEvent) override;

	/**
	*	Traverses this widget's hierarchy to setup all the UIUINavButtons
	*/
	void TraverseHierarchy();

	/**
	*	Reconfigures the blueprint if it has already been setup
	*/
	void ReconfigureSetup();
	
	/**
	*	Called manually to setup all the elements in the Widget
	*/
	virtual void UINavSetup();

	UFUNCTION(BlueprintNativeEvent, Category = UINavWidget)
	UUINavComponent* GetInitialFocusComponent();
	virtual UUINavComponent* GetInitialFocusComponent_Implementation();

	void PropagateGainNavigation(UUINavWidget* PreviousActiveWidget, UUINavWidget* NewActiveWidget, const UUINavWidget* const CommonParent);

	virtual void GainNavigation(UUINavWidget* PreviousActiveWidget);

	/**
	*	Called when navigation is gained
	*/
	UFUNCTION(BlueprintNativeEvent, Category = UINavWidget)
    void OnGainedNavigation(UUINavWidget* PreviousActiveWidget, const bool bFromChild);
	
	virtual void OnGainedNavigation_Implementation(UUINavWidget* PreviousActiveWidget, const bool bFromChild);

	void PropagateLoseNavigation(UUINavWidget* NewActiveWidget, UUINavWidget* PreviousActiveWidget, const UUINavWidget* const CommonParent);
	
	virtual void LoseNavigation(UUINavWidget* NewActiveWidget);

	/**
	*	Called when navigation is lost
	*/
	UFUNCTION(BlueprintNativeEvent, Category = UINavWidget)
        void OnLostNavigation(UUINavWidget* NewActiveWidget, const bool bToChild);
	virtual void OnLostNavigation_Implementation(UUINavWidget* NewActiveWidget, const bool bToChild);

	void SetCurrentComponent(UUINavComponent* Component);
	void SetHoveredComponent(UUINavComponent* Component);
	void SetSelectedComponent(UUINavComponent* Component);

	void UpdateNavigationVisuals(UUINavComponent* Component, const bool bBypassForcedNavigation = false);

	/**
	*	Changes the selector's location to that of the button with the given index in the Button's array
	*
	*	@param	Index  The new button's index in the Button's array
	*/
	UFUNCTION(BlueprintCallable, Category = UINavWidget)
	void UpdateSelectorLocation(UUINavComponent* Component);

	/**
	*	Plays the animations in the UINavAnimations array
	*
	*	@param	From  The index of the button that was navigated from
	*	@param	To  The index of the button that was navigated to
	*/
	void ExecuteAnimations(UUINavComponent* FromComponent, UUINavComponent* ToComponent);

	/**
	*	Changes the new text and previous text's colors to the desired colors
	*
	*	@param	Index  The new button's index in the Button's array
	*/
	void UpdateButtonStates(UUINavComponent* Component);

	/**
	*	Changes the new text and previous text's colors to the desired colors
	*
	*	@param	Index  The new button's index in the Button's array
	*/
	void UpdateTextColor(UUINavComponent* Component);

	/**
	*	Changes the selector's scale to the scale given
	*
	*	@param	NewScale  The selector's new scale
	*/
	UFUNCTION(BlueprintCallable, Category = UINavWidget)
	void SetSelectorScale(FVector2D NewScale);

	/**
	*	Changes the selector's visibility
	*
	*	@param	bVisible Whether the selector will be visible
	*/
	UFUNCTION(BlueprintCallable, Category = UINavWidget)
	void SetSelectorVisibility(const bool bVisible);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = UINavWidget)
	bool IsSelectorVisible();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = UINavWidget)
	FORCEINLINE bool IsRebindingInput() const { return ReceiveInputType != EReceiveInputType::None; }

	/**
	*	Called when the button with the specified index was navigated upon
	*
	*	@param	From  The index of the button that was navigated from
	*	@param	To  The index of the button that was navigated to
	*/
	UFUNCTION(BlueprintNativeEvent, Category = UINavWidget)
	void OnNavigate(UUINavComponent* FromComponent, UUINavComponent* ToComponent);
	
	virtual void OnNavigate_Implementation(UUINavComponent* FromComponent, UUINavComponent* ToComponent);

	/**
	*	Notifies that a button was selected, and indicates its index
	*
	*	@param	Index  The index of the button that was selected
	*/
	UFUNCTION(BlueprintNativeEvent, Category = UINavWidget)
	void OnSelect(UUINavComponent* Component);
	
	virtual void OnSelect_Implementation(UUINavComponent* Component);

	void PropagateOnSelect(UUINavComponent* Component);

	UFUNCTION(BlueprintNativeEvent, Category = UINavWidget)
	void OnStartSelect(UUINavComponent* Component);
	
	virtual void OnStartSelect_Implementation(UUINavComponent* Component);

	void PropagateOnStartSelect(UUINavComponent* Component);

	UFUNCTION(BlueprintNativeEvent, Category = UINavWidget)
	void OnStopSelect(UUINavComponent* Component);

	void PropagateOnStopSelect(UUINavComponent* Component);
	
	virtual void OnStopSelect_Implementation(UUINavComponent* Component);

	void AttemptUnforceNavigation(const EInputType NewInputType);

	void ForceNavigation();
	void UnforceNavigation();

	/**
	*	Called when ReturnToParent is called (i.e. the player wants to exit the menu)
	*/
	UFUNCTION(BlueprintNativeEvent, Category = UINavWidget)
	void OnReturn();
	
	virtual void OnReturn_Implementation();

	/**
	*	Called when player navigates to the next section
	*/
	UFUNCTION(BlueprintNativeEvent, Category = UINavWidget)
	void OnNext();
	
	virtual void OnNext_Implementation();

	void PropagateOnNext();

	/**
	*	Called when player navigates to the previous section
	*/
	UFUNCTION(BlueprintNativeEvent, Category = UINavWidget)
	void OnPrevious();
	
	virtual void OnPrevious_Implementation();

	void PropagateOnPrevious();

	/**
	*	Called when the input type changed
	*/
	UFUNCTION(BlueprintNativeEvent, Category = UINavWidget)
	void OnInputChanged(const EInputType From, const EInputType To);
	
	virtual void OnInputChanged_Implementation(const EInputType From, const EInputType To);

	/**
	*	Called before this widget is setup for UINav logic
	*/
	UFUNCTION(BlueprintNativeEvent, Category = UINavWidget)
	void PreSetup(const bool bFirstSetup);
	
	virtual void PreSetup_Implementation(const bool bFirstSetup);

	/**
	*	Called when this widget completed UINavSetup
	*/
	UFUNCTION(BlueprintNativeEvent, Category = UINavWidget)
	void OnSetupCompleted();
	
	virtual void OnSetupCompleted_Implementation();

	/**
	*	Called when the user navigates left on a UINavComponentBox
	*/
	UFUNCTION(BlueprintNativeEvent, Category = UINavWidget)
	void OnHorizCompNavigateLeft(UUINavComponent* Component);
	
	virtual void OnHorizCompNavigateLeft_Implementation(UUINavComponent* Component);

	/**
	*	Called when the user navigates right on a UINavComponentBox
	*/
	UFUNCTION(BlueprintNativeEvent, Category = UINavWidget)
	void OnHorizCompNavigateRight(UUINavComponent* Component);
	
	virtual void OnHorizCompNavigateRight_Implementation(UUINavComponent* Component);

	/**
	* Called when a HorizontalComponent was updated
	*/
	UFUNCTION(BlueprintNativeEvent, Category = UINavWidget)
	void OnHorizCompUpdated(UUINavComponent* Component);
	
	virtual void OnHorizCompUpdated_Implementation(UUINavComponent* Component);

	virtual void NavigatedTo(UUINavComponent* NavigatedToComponent, const bool bNotifyUINavPC = true);

	void CallOnNavigate(UUINavComponent* FromComponent, UUINavComponent* ToComponent);

	virtual void StartedSelect();
	virtual void StoppedSelect();

	virtual void StartedReturn();
	virtual void StoppedReturn();

	bool TryConsumeNavigation();

	bool IsForcingNavigation() const { return bForcingNavigation; }

	bool IsBeingRemoved() const { return bBeingRemoved; }

	UUINavComponent* GetCurrentComponent() const { return CurrentComponent; }

	template<typename T>
	static T* GetOuterObject(const UObject* const Object);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = UINavWidget)
	UUINavWidget* GetMostOuterUINavWidget();

	UUINavWidget* GetChildUINavWidget(const int ChildIndex) const;

	FORCEINLINE TArray<int> GetUINavWidgetPath() const { return UINavWidgetPath; }

	void AddParentToPath(const int IndexInParent);

	UUINavComponent* GetFirstComponent() const { return FirstComponent; }

	void SetFirstComponent(UUINavComponent* Component);

	void RemovedComponent(UUINavComponent* Component);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = UINavWidget)
	bool IsSelectorValid();

	FORCEINLINE uint8 GetSelectCount() const { return SelectCount; }

	/**
	*	Adds given widget to screen (strongly recommended over manual alternative)
	*
	*	@param	NewWidgetClass  The class of the widget to add to the screen
	*	@param	bRemoveParent  Whether to remove the parent widget (this widget) from the viewport
	*	@param  bDestroyParent  Whether to destruct the parent widget (this widget)
	*	@param  ZOrder Order to display the widget
	*/
	UFUNCTION(BlueprintCallable, Category = UINavWidget, meta = (AdvancedDisplay=2))
	UUINavWidget* GoToWidget(TSubclassOf<UUINavWidget> NewWidgetClass, const bool bRemoveParent, const bool bDestroyParent = false, const int ZOrder = 0);

	/**
	*	Adds given widget to screen (strongly recommended over manual alternative)
	*
	*	@param	NewWidgetClass  The class of the widget to add to the screen
	*	@param	bRemoveParent  Whether to remove the parent widget (this widget) from the viewport
	*	@param  bDestroyParent  Whether to destruct the parent widget (this widget)
	*	@param  ZOrder Order to display the widget
	*/
	UFUNCTION(BlueprintCallable, Category = UINavWidget, meta = (AdvancedDisplay = 2))
	UUINavWidget* GoToPromptWidget(TSubclassOf<UUINavPromptWidget> NewWidgetClass, const FPromptWidgetDecided& Event, const bool bRemoveParent, const int ZOrder = 0);

	/**
	*	Adds given widget to screen (strongly recommended over manual alternative)
	*
	*	@param	NewWidget  Object instance of the UINavWidget to add to the screen
	*	@param	bRemoveParent  Whether to remove the parent widget (this widget) from the viewport
	*	@param  bDestroyParent  Whether to destruct the parent widget (this widget)
	*	@param  ZOrder Order to display the widget
	*/
	UFUNCTION(BlueprintCallable, Category = UINavWidget, meta = (AdvancedDisplay=2))
	UUINavWidget* GoToBuiltWidget(UUINavWidget* NewWidget, const bool bRemoveParent, const bool bDestroyParent = false, const int ZOrder = 0);

	/**
	*	Adds this widget's parent to the viewport (if applicable)
	*	and removes this widget from viewport
	*/
	UFUNCTION(BlueprintCallable, Category = UINavWidget, meta = (AdvancedDisplay = 1))
	virtual void ReturnToParent(const bool bRemoveAllParents = false, const int ZOrder = 0);

	void RemoveAllParents();

	int GetWidgetHierarchyDepth(UWidget* Widget) const;

	FORCEINLINE bool HasNavigation() const { return bHasNavigation; }

	void OnHoveredComponent(UUINavComponent* Component);
	void OnUnhoveredComponent(UUINavComponent* Component);

	void OnPressedComponent(UUINavComponent* Component);
	void OnReleasedComponent(UUINavComponent* Component);

};
