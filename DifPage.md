Erampage has the map patching feature. When the game loads a `*.map`, it looks for a `*.dif` in the same directory. If a `*.dif` file is found, it is handled as a patch and applied to the `*.map` upon loading. So the `*.map` doesn't need to be changed, it can even peacefully remain within its `*.grp` container.

In order to produce `*.dif`, one can use _[xdelta](http://code.google.com/p/xdelta/)_ utility, which code I used in erampage. The usual command would be

> `xdelta3 -s E1L1.MAP E1M1.MAP > E1L1.DIF`

Should the game crash with `*.dif` produced by xdelta, you can use my own utility, _[bindiff](http://erampage.googlecode.com/files/bindiff.zip)_, which is based on the very same code (to the letter) I used in erampage. The syntax would then be

> `bindiff diff E1L1.MAP E1M1.MAP E1L1.DIF`

NB: sizes of `*.dif` patches produced by _xdelta_ and _bindiff_ are usually slightly different. And they usually both work with erampage.

In case you need to restore the patched map as a file (ex. to open it in an editor), use the following command:

> `xdelta3 -d -s E1L1.MAP E1L1.DIF > E1M1.MAP`

> `bindiff patch E1L1.MAP E1L1.DIF E1M1.MAP`

It is technically possible to use the same technique to hack other game resources (tiles, palettes, etc), although this is not implemented now.