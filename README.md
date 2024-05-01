# GWCA #

A C++ API to interface with the Guild Wars client.

### Team ###

https://github.com/orgs/gwdevhub/people

### Credits to: ###

ACB, Harboe - For their work in the original GWCA project, most agentarray and ctogs functions are derived from this work
ACB, _rusty, miracle444 - GWLP:R work with StoC packets, StoC handler derived form the GWLP Dumper
Miracle444, TheArkanaProject - Work done in the GWA2 Project.

### Usage with cmake ###

Using cmake makes using GWCA simple:

```cmake
include_guard()
include(FetchContent)

FetchContent_Declare(
  gwca
  GIT_REPOSITORY https://github.com/gwdevhub/gwca
  GIT_TAG master
)
FetchContent_GetProperties(gwca)
FetchContent_Populate(gwca)

add_subdirectory(${gwca_SOURCE_DIR} EXCLUDE_FROM_ALL)
```

Then you can reference GWCA in your own project.

### Usage without cmake ###

Start up a project in Visual Studio and init a git repo inside for it. Then use:

```
git submodule add https://github.com/gwdevhub/GWCA
```

To clone the repo. From here, include the project (.vcxproj) in your solution using Add->Existing project, add a dependency to your main project, And add the project as a reference in your main project. Now when your main project compiles, GWCA++ will compile into it. From here just include "APIMain.h" into your main file.

### Usage in code ###

You must always start with calling the GW::Initialize() function, this function is what scans memory and places hooks.
From there you can retrieve different submodules such as Agents, Items, Skillbar, Effects, Map, etc.
To terminate GWCA, remove all of your own hooks and then call GW::Terminate.

To get more insights into use of GWCA, check out the [GWToolbox](https://github.com/gwdevhub/GWToolboxpp) project.