//
// VideoCoreRTC.Build.cs
//
//  Generated on Feb 8 2021
//  Template created by Peter Gusev on 27 January 2020.
//  Copyright 2013-2021 Regents of the University of California
//

using System.IO;
using UnrealBuildTool;
using System;
using System.Collections.Generic;

public class VideoCoreRTC : ModuleRules
{
	private string ModulePath
	{
		get
		{
			return ModuleDirectory;
		}
	}

	public VideoCoreRTC(ReadOnlyTargetRules Target) : base(Target)
	{
		CppStandard = CppStandardVersion.Cpp17;
		bEnableExceptions = true;
		bUseRTTI = true;
		UnsafeTypeCastWarningLevel = WarningLevel.Off;

		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"UMG",
				"VideoCore",
				"DDBase",
				"DDLog",
				"SocketIOClient", 
				"SocketIOLib", 
				"Json", 
				"SIOJson",	
				"CoreUtility"
				// ... add other public dependencies that you statically link with here ...
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
                "RenderCore",
                "RHI",
                "Slate",
				"SlateCore",
				"Media",
				"MediaAssets"
				// ... add private dependencies that you statically link with here ...
			}
			);

        SetupThirdPartyLibs();

    }

    public void SetupThirdPartyLibs()
    {
        // here, we set paths and linkage to any thirdparty libraries our plugin uses

        // discover dependencies automatically
        foreach (var path in Directory.GetDirectories(ThirdPartyPath, "*"))
        {
            string libName = Path.GetFileName(path.TrimEnd(Path.DirectorySeparatorChar));
            SetupThirdParty(libName);
        }

        //bool isDebug = (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT);

        SetupWebrtc();
    }

    private string ThirdPartyPath
    {
        get { return Path.GetFullPath(Path.Combine(ModuleDirectory, "../ThirdParty/")); }
    }

    public string GetLibPath(string libName)
    {
        return Path.Combine(ThirdPartyPath, libName);
    }

    public string GetLibIncludePath(string libPath)
    {
        return Path.Combine(libPath, "include");
    }

    public string GetLibBinaryPath(string libPath)
    {
        string path = Path.Combine(libPath, "lib");

        if (Target.Platform == UnrealTargetPlatform.Mac)
            return Path.Combine(path, "macOS");

        if (Target.Platform == UnrealTargetPlatform.Win64)
            return Path.Combine(path, "win/x64");

        return path;
    }

    public List<string> GetAllLibBinaries(string libBinaryPath)
    {
        string[] bins = (Target.Platform == UnrealTargetPlatform.Win64) ?
             Directory.GetFiles(libBinaryPath, "*.lib") :
             Directory.GetFiles(libBinaryPath, "lib*.dylib");

        List<string> binaries = new List<string>();

        foreach (var s in bins)
            binaries.Add(Path.GetFileName(s));

        return binaries;
    }

    private void SetupWebrtc()
    {
        CppStandard = CppStandardVersion.Cpp17;

        PublicDefinitions.Add("USE_AURA=1");
        PublicDefinitions.Add("__STD_C");
        PublicDefinitions.Add("_CRT_RAND_S");
        PublicDefinitions.Add("_CRT_SECURE_NO_DEPRECATE");
        PublicDefinitions.Add("_SCL_SECURE_NO_DEPRECATE");
        PublicDefinitions.Add("_ATL_NO_OPENGL");
        PublicDefinitions.Add("_WINDOWS");
        PublicDefinitions.Add("CERT_CHAIN_PARA_HAS_EXTRA_FIELDS");
        PublicDefinitions.Add("PSAPI_VERSION=2");
        PublicDefinitions.Add("WIN32");
        PublicDefinitions.Add("_SECURE_ATL");
        PublicDefinitions.Add("WINUWP");
        PublicDefinitions.Add("__WRL_NO_DEFAULT_LIB__");
        PublicDefinitions.Add("WINAPI_FAMILY=WINAPI_FAMILY_PC_APP");
        PublicDefinitions.Add("WIN10=_WIN32_WINNT_WIN10");
        PublicDefinitions.Add("WIN32_LEAN_AND_MEAN");
        PublicDefinitions.Add("NOMINMAX");
        PublicDefinitions.Add("_UNICODE");
        PublicDefinitions.Add("UNICODE");
        PublicDefinitions.Add("NTDDI_VERSION=NTDDI_WIN10_RS2");
        PublicDefinitions.Add("_WIN32_WINNT=0x0A00");
        PublicDefinitions.Add("WINVER=0x0A00");
        PublicDefinitions.Add("NDEBUG");
        PublicDefinitions.Add("NVALGRIND");
        PublicDefinitions.Add("DYNAMIC_ANNOTATIONS_ENABLED=0");
        PublicDefinitions.Add("WEBRTC_ENABLE_PROTOBUF=0");
        PublicDefinitions.Add("WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE");
        PublicDefinitions.Add("RTC_ENABLE_VP9");
        PublicDefinitions.Add("HAVE_SCTP");
        PublicDefinitions.Add("WEBRTC_LIBRARY_IMPL");
        PublicDefinitions.Add("WEBRTC_NON_STATIC_TRACE_EVENT_HANDLERS=0");
        PublicDefinitions.Add("WEBRTC_WIN");
        PublicDefinitions.Add("ABSL_ALLOCATOR_NOTHROW=1");
        PublicDefinitions.Add("HAVE_SCTP");
        PublicDefinitions.Add("WEBRTC_VIDEO_CAPTURE_WINRT");


        string webrtcPath = GetLibIncludePath(GetLibPath("webrtc"));

        // list of modules to include https://webrtc.googlesource.com/src/+/HEAD/native-api.md
        PublicIncludePaths.Add(webrtcPath);
        PublicIncludePaths.Add(Path.Combine(webrtcPath, "third_party/abseil-cpp"));
        PublicIncludePaths.Add(Path.Combine(webrtcPath, "third_party/libyuv/include"));
    }

    private void SetupThirdParty(string libName)
    {
        // set up include paths for dependencies
        System.Console.WriteLine("Adding include path for {0}: {1}", libName, GetLibIncludePath(GetLibPath(libName)));
        PublicIncludePaths.Add(GetLibIncludePath(GetLibPath(libName)));

        if (!IsHeaderOnlyLibrary(libName))
        {
            System.Console.WriteLine("Setup binary linkage for {0}", libName);
            // set up linkage
            // if our thirdparty libraries have any prebuilt binaries, add them now
            // to PublicDelayLoadDLLs and RuntimeDependencies
            if (Target.Platform == UnrealTargetPlatform.Win64)
            {
                string binPath = GetLibBinaryPath(GetLibPath(libName));
                List<string> binaries = GetAllLibBinaries(binPath);

                foreach (var b in binaries)
                {
                    System.Console.WriteLine("Add linking {0}: {1}", libName, Path.Combine(binPath, b));
                    PublicAdditionalLibraries.Add(Path.Combine(binPath, b));
                }
            }
            else if (Target.Platform == UnrealTargetPlatform.Android)
            {
                // set up Android lib paths
            }
            else if (Target.Platform == UnrealTargetPlatform.IOS)
            {
                // set up iOS lib paths
            }
            else if (Target.Platform == UnrealTargetPlatform.Mac)
            {
                string binPath = GetLibBinaryPath(libName);
                List<string> binaries = GetAllLibBinaries(GetLibBinaryPath(libName));

                foreach (var b in binaries)
                {
                    //System.Console.WriteLine("Adding linking {0}: {1}", libName, Path.Combine(binPath, b));
                    PublicDelayLoadDLLs.Add(Path.Combine(binPath, b));
                    RuntimeDependencies.Add(Path.Combine(binPath, b));
                }
            }
        }
    }

    private bool IsHeaderOnlyLibrary(string libName)
    {
        return !Directory.Exists(GetLibBinaryPath(GetLibPath(libName)));
    }
}
