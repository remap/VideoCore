//
// video-core-rtc.hpp
//
//  Generated on February 18 2020
//  Copyright 2021 Regents of the University of California
//

#pragma once

#include "CoreMinimal.h"

#pragma warning(disable:4596)
#pragma warning(disable:4800)
#include "mediasoupclient.hpp"
#include "api/create_peerconnection_factory.h"

namespace videocore 
{
	void initialize();

	nlohmann::json fromFString(FString jsonString);
	nlohmann::json fromFJsonValue(const TSharedPtr<FJsonValue>& jsonValue);
	nlohmann::json fromFJsonObject(const TSharedPtr<FJsonObject>& jsonObject);

	TSharedPtr<FJsonValue> fromJsonObject(const nlohmann::json& jobj);
	
	mediasoupclient::Device& getDevice();
	rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> getWebRtcFactory();

	void loadMediaSoupDevice(TSharedPtr<FJsonValue> rtpCapabilities);
}