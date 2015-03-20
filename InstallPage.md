For Linux instructions, [scroll down](#Linux.md).


# Windows #

Important: this procedure applies to the folder structure from the Windows binary ([package](http://www.jonhunt.com/redneck/erampage.html)). The folder where **erampage.exe** resides is called the top folder, subfolders are referred by names, e.g. _REDNECK_, _ROUTE66_ etc.

## Redneck Rampage & Redneck Rampage Rides Again ##
If you have the RRRA game, you should put the **REDNECK.GRP** file from that game in the top folder. The **REDNECK.GRP** file from Redneck Rampage goes to the _REDNECK_ subfolder.

If you don't have the RRRA, put the **REDNECK.GRP** file from Redneck Rampage in the top folder.

## Suckin' Grits on Route 66 ##
In order to run this, you need to have RR installed in the top folder. All resources from this addon go to the _ROUTE66_ subfolder.

## Cuss Pack addon ##
The file **CUSSPACK.GRP** must be placed in the top folder, and the game must be started with command
```
erampage /gCUSSPACK.grp
```

## Redneck Deer Huntin' ##
Not supported yet.


# Linux #

  1. Create a folder named ~/.erampage
  1. Extract the windows package file into it: `cd ~/.erampage; 7z x erampage.7z`
  1. Copy `TABLES.DAT` and `REDNECK.GRP` from Redneck disk into ~/.erampage
  1. Rename them to be lowercase (also rename to lowercase LOOKUP.DAT from the pack)
  1. For add-ons like Rides Again, the instructions for [Windows](#Windows.md) apply.
[back to main](MainPage.md)