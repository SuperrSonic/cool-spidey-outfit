Cool Spidey Outfit - Image Format Instructions


When adding your own spidey suit TPL, you can use BrawlBox for simplicity.

However, for the city banners, BrawlBox's image quality when converting textures
is somewhat lacking. To avoid this I recommend gxtexconv.

If you have devkitppc installed, you can change CMPR.cmd in notepad to use
your installed gxtexconv, but for convenience I've included the tool here.

All city banners are 128x128, once you have your file ready, drag and drop it
into CMPR.cmd, it should save the TPL file and an .h file for use with homebrew
you can go ahead and remove the .h file.

Now rename the file as tri1.tpl, tri2.tpl, or tri3.tpl

For the title screen and the title screen mask textures, do the same but with
its original width and height and name the TPL as title.tpl and titlemask.tpl
once you change spidey suits it will detect the files and patch them into the ISO.

The paths are hardcoded, so they must be in:
device:/apps/Cool Spidey Outfit/title.tpl
