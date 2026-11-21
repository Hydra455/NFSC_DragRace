## NFS:C Drag Races - Restored Cut Contend Mod

## Installation
1. Download NFS-VltEd and Binary (NOT BINARIUS)
2. Open NFS-VltEd and select NFS Carbon directory
3. Click on File > Import Modscript > _setup_vlted.nfsms
4. Click on ,,Install Drag Races" (if you already have the mod you can use ,,Uninstall Drag Races")
5. Click on File > Save or Ctrl+S to save the changes
6. Close NFS-VltEd and open Binary
7. Select ,,User Mode" and select the ,,_setup_binary.end"
8. Select your NFS Carbon directory and save the changes
9. Put NFSC_DragRace.asi and the DragHUD folder in your scripts folder (make sure you have dinput8.dll)
10. Make a new save game or keep the current one and enjoy the mod

## Discord
For any issues or suggestions for the mod you can join my discord server and we can talk there
Discord Link: https://discord.com/invite/CXcVacRsX2


## Building this update

Open NFSC_DragRace.sln in Visual Studio 2026 and build Release | x86. Copy bin/Release/NFSC_DragRace.asi and the DragHUD folder into Carbon's scripts folder.

This update removes the executable SHA-256 allowlist and the fixed PE header position. It reads the loaded PE header dynamically. Native hook and patch checks report the affected address if the required code differs. The HUD file checks validate the packaged assets.

This package contains the updated source and HUD assets. Rebuild the ASI to install the executable compatibility change.
