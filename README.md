# erampage
Automatically exported from code.google.com/p/erampage

# Introduction

eRampage is a free open source port intended to run games from Redneck series. Those include Redneck Rampage, Suckin' Grits on Route 66, Redneck Rampage Rides Again and Redneck Deer Huntin'.

# Details

In order to get the port working, you will need to go through the following steps:

Getting eRampage (binary and sources)

Here's what you need to download:

## Binary files

This package contains Windows binaries, as well as some resources needed for the game. This is the only file required for Windows users. Un-pack it and proceed with installing game files. If you can't open the package, download 7-Zip archiver.

Linux users will also need this file, as it contains resources needed for the game. (TODO: we really should do something so that the Linux users wouldn't have to download Windwoes binaries)

## Source code

Linux users are required to download the sources to build the game. Windows users may also download the sources and build the binary executable themselves if they want to.

In order do download the sources, go to the Source page and follow the instructions there. Windows users may need to install SVN client before they could download the sources.

Note that erampage and its ancestors (eduke32 and the Build engine) are licensed with GPL, which means that you have the right to examine, modify and redistribute the engine source code, but NOT the game content. How to distinguish them? The stuff you download from the SVN repository is free, the stuff that comes from your Redneck Rampage disks is not free. TODO: figure out if erampage.7z is free or not.

## Redneck Rampage data files

You need these files from the disk:

redneck.grp
tables.dat
lookup.dat

# Building eRampage (Windows users may skip this step)

## Quick instructions for Linux

type 'make' and enjoy

## See eRampage

Building eRampage is very similar to building Eduke32. Because of this, I advise you to read Eduke32 wiki about building executables under Windows and Linux. Please note the differences:

Mapster32 won't compile in eRampage installation. This is not a build problem.
Paths to game files under Linux have been changed, so they won't interfere with Eduke32

# Installing game resources

## Windows

Important: this procedure applies to the folder structure from the Windows binary (package). The folder where erampage.exe resides is called the top folder, subfolders are referred by names, e.g. REDNECK, ROUTE66 etc.

### Redneck Rampage & Redneck Rampage Rides Again

If you have the RRRA game, you should put the REDNECK.GRP file from that game in the top folder. The REDNECK.GRP file from Redneck Rampage goes to the REDNECK subfolder.

If you don't have the RRRA, put the REDNECK.GRP file from Redneck Rampage in the top folder.

### Suckin' Grits on Route 66

In order to run this, you need to have RR installed in the top folder. All resources from this addon go to the ROUTE66 subfolder.

### Cuss Pack addon

The file CUSSPACK.GRP must be placed in the top folder, and the game must be started with command

erampage /gCUSSPACK.grp
Redneck Deer Huntin'
Not supported yet.

## Linux

Create a folder named ~/.erampage
Extract the windows package file into it: cd ~/.erampage; 7z x erampage.7z
Copy TABLES.DAT and REDNECK.GRP from Redneck disk into ~/.erampage
Rename them to be lowercase (also rename to lowercase LOOKUP.DAT from the pack)
For add-ons like Rides Again, the instructions for Windows apply.

# Credits

Erampage is based on EDuke32 (http://eduke32.com/) Map patches are made by Jon Hunt (http://www.jonhunt.com/redneck/)
