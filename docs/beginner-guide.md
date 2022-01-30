# Beginner guide

> As a sidenote - I am not a linux/wayland/wlr/tiling manager expert, vivarium is the first one that I use
> I think it makes me a good person to explain what I had to do on top of what README says to use vivarium
> as a daily driver

## I ran vivarium... what's next?

(I am assuming that you weren't modifying any part of the config)

So - you were able to get into the a blank screen, possibly with a Waybar on the top?
Great, most likely you will have to exit vivarium now for a second.
To exit vivarium you will have to type capital Q while holding Alt key... so Alt+Shift+Q
Check if you have `alacritty` (https://alacritty.org) installed, it is needed when you are using default config.

Ok, you have alacritty, now run vivarium once again.
The most common vivarium functions that you will be using is to spawn a new tile and to move between workspaces.
(Keep in mind that key name "Return" is kinda another way of saying "Enter")
Spawning a new tile is Meta+Shift+Return (or Meta+Shift+T)
Jumping between workspaces is Meta+<number>

### What is "Meta" again?

Meta is by default the Alt key, you can change it in the config.
When you hold Meta - vivarium checks if your keypresses aren't some keybind.

### Okay, I get it, tell me more about workspaces

Perfect, when you run vivarium - your workspace is "first workspace", so if you spawn a new tile here,
then you will do Meta+2, you will be in 2nd workspace... to go back to your tile - you have to go to
first workspace -> Meta+1

### Practical example

Practical example - since I am a webdev... I usually have to open editor, web browser and a dev server.
So!
After running vivarium, I:
1. Spawn a new tile (Meta+Shift+Return) - alacritty comes up.
2. Navigate to project folder, run an editor here.
3. Jump to 2nd workspace (Meta+2)
4. Spawn a new tile. Run firefox here (Just type "firefox" and hit enter)
5. Jump to 3rd workspace (Meta+3)
6. Spawn a new tile. Navigate to project folder and run dev server.
7. Go back to workspace with firefox (Meta+2) and connect with dev server.
8. Go back to editor (Meta+1) and start deving.

... After listing that I think that I have to do a script for that, maybe I will add it here in future
after figuring out how such script can be done :)

