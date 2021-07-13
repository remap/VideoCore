// Fill out your copyright notice in the Description page of Project Settings.


#include "VideoCoreCompositingInput.h"

void
UVideoCoreCompositingInput::SetMediaTextureSource(UTexture* texture)
{
	//Cast<UMediaTexture*>
	MediaSource = (UMediaTexture*)texture;
}