# WebView2 SDK headers

`include/WebView2.h` and `include/WebView2EnvironmentOptions.h` from
Microsoft's WebView2 SDK, NuGet package `Microsoft.Web.WebView2`
version 1.0.4191.47
(https://www.nuget.org/packages/Microsoft.Web.WebView2/1.0.4191.47),
unchanged. License: LICENSE.txt (BSD-style), NOTICE.txt.

They are what the Windows build of xppautX's own window (docs/roadmap.md
W13) compiles against. Nothing else from the package is used: the
`webview` library's built-in loader replaces WebView2Loader.dll, and the
WebView2 runtime itself ships with Windows 10/11 and Edge.
