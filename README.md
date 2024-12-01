# Cool Spidey Outfit
 Suit changer for Spider-Man 2.


This patches the Spider-Man 2 (GameCube) game to change the texture for the spidey suit directly from a Wii or GameCube with a GCLoader/CubeODE, or an SD2SP2 available.

Modifying the ISO is the only way to quickly change this data, as the suit texture is part of a huge asset file.


#Howto

If you're on a Wii, loading this from the Homebrew channel, it will look for the game's ISO in "sd:/games/Spider-Man 2 [GK2E52]/game.iso"

You can edit meta.xml to change the path, and to use "usb:/" instead.

For GameCube, it will load "sd:/games/Spider-Man 2.iso", use a dcp file to send arguments with swiss to change the path to start with "sp:/" to use SD2SP2.


#Known issues

SD2SP2 is not tested.

"/apps/Cool Spidey Outfit/" needs to exist to change the suit from an external TPL texture, as well as the 3 city banners, named: tri1.tpl, tri2.tpl, tri3.tpl.


#Preview video:
https://www.youtube.com/watch?v=CugpL1lJIAY
