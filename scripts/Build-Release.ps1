# This script is just a template and has to be copied and modified per project
# This script should be called from .vscode/tasks.json with
#
#   scripts/Build-Release.ps1            - for Beta builds
#   scripts/Build-Release.ps1 Release    - for Release builds
#
# {
#     "label": "Build-Release",
#     "type": "shell",
#     "command": "scripts/Build-Release.ps1 Release",
#     "args": [],
#     "problemMatcher": [],
#     "group": "test"
# },
# {
#     "label": "Build-Beta",
#     "type": "shell",
#     "command": "scripts/Build-Release.ps1 ",
#     "args": [],
#     "problemMatcher": [],
#     "group": "test"
# }



# set product names, allows mapping of (devel) name in Project to a more consistent name in release
$settings = scripts/OpenKNX-Build-Settings.ps1 $args[0]

# execute generic pre-build steps
lib/OGM-Common/scripts/setup/reusable/Build-Release-Preprocess.ps1 $args[0]
if (!$?) { exit 1 }

# build firmware based on generated headerfile 
# the following build steps are project specific and must be adopted accordingly
# see comment in Build-Step.ps1 for argument description
if ($($settings.releaseIndication) -eq "Release") {
  Write-Host "Building Release"
  ./lib/OGM-Common/scripts/setup/reusable/Build-Step.ps1 release_KNX2HOVALGATEWAY firmware-OpenKNX-KNX2HovalGateway uf2
  if (!$?) { exit 1 }
}
else {
  Write-Host "Building $($settings.releaseIndication)"
  ./lib/OGM-Common/scripts/setup/reusable/Build-Step.ps1 dev_KNX2HOVALGATEWAY firmware-OpenKNX-KNX2HovalGateway uf2
  if (!$?) { exit 1 }
}

# execute generic post-build steps
lib/OGM-Common/scripts/setup/reusable/Build-Release-Postprocess.ps1 $args[0]
if (!$?) { exit 1 }
