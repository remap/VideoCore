# VideoCore Plugin

## Description

TBD

## Dependencies

This plugin has multiple dependencies:

**UE Plugins**

* [DDTools](https://github.com/remap/DDTools) -- base plugin code, logging functionnality (clone recursively);
* [NDIIOPlugin](https://github.com/remap/NDIIOPlugin) -- NDI support (**Windows-only**);
* [WebCameraFeed](https://github.com/bakjos/WebCameraFeed) -- live connected cameras support;
* [socketio-client-ue4 (tls branch)](https://github.com/peetonn/socketio-client-ue4/tree/tls) -- socket.io for media server signaling (clone recursively).

**Third-party Libraries**

> ⚠️  All third-party dependencies below, compiled for Win64 platform can be downloaded from [here](https://bintray.com/peetonn/grasshopper/videocore_bin/0.0.1-alpha/view/files#files/). Zip archive must be extracted into `VideoCore/Source/ThirdParty` folder.
* [libmediasoupclient](https://github.com/versatica/libmediasoupclient/) -- mediasoup media server API;
* libsdptransform -- SDP helper code;
* [webrtc](https://webrtc.github.io/webrtc-org/native-code/development/) -- C++ WebRTC library for low-latency data streaming support.

**WebRTC support -- Media Server**

In order to use RTC functionality, a media server is required. Read [here](https://mediasoup.org/documentation/overview/) on what media server is and why it's needed. [mediasoup](https://mediasoup.org/) library was used for implementing media server for this plugin.
A node.js implementation of media server can be found here:
* [videocore-media-server (node.js)](https://github.com/peetonn/videocore-media-server)
