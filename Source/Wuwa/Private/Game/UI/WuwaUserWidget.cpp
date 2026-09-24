// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/UI/WuwaUserWidget.h"

void UWuwaUserWidget::InitWidgetController(UObject* widgetcontroller)
{
	WidgetController = widgetcontroller;
	AfterSetWidgetController();
}
