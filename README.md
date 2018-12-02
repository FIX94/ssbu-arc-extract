# ssbu-arc-extract  
Aims to extract various files from the ssbu data.arc.  
See the "releases" tab for a per-built executable.  
You can put the executable right next to the data.arc and then use this tool in 2 different modes.    

Mode 1:  
When you just run it without any arguments, it will output all music into a "lopus" and all video into a "webm" folder.  
The .lopus music files can be played back with any player that has the vgmstream plugin and the videos can be played back in any video player, I tried mpv for this and it works fine.  
Some of the video files also contain multiple audio language tracks that you can switch to in your video player.    
The music actually gets automatically extracted from their original .nus3audio files to make them listenable, if you want the raw .nus3audio container, see mode 2 below.    

Mode 2:  
When you run it from command line, you can give it a filename with path as argument, it will then try and locate that path in the file, extract it when found and also attempt to decompress it if it finds a compression flag.  
The file will just be placed in the same folder as the data.arc.  
I'm not sure where those filenames are actually located, to give you a simple example of such a filepath:  
sound/bank/common/se_common.nus3audio  
Also you can individually extract the music and video files that get extrated above using the stream:/ prefix, so a path like this for example:  
stream:/sound/bgm/bgm_crs2_49_commonsilent_lp.nus3audio    

About the full music and video file structure:  
All music filenames are kept in a database in another file you can grab using mode 2:  
ui/param/database/ui_bgm_db.prc  
that database just contains the base names, you still have to build the full path like this:  
stream:/sound/bgm/bgm_<name_from_that_list>.nus3audio  
The videos have 2 databases, the first one with opening videos and cutscense being located at:  
ui/param/database/ui_movie_db.prc  
And that already contains the full filenames with the path and stream:/ prefix.  
More video files can be found in this database:  
ui/param/database/ui_technic_db.prc  
This points to the videos that contain most various demonstration videos, you have to build the path like this:  
stream:/movie/technique/<name_from_that_list>.webm    

Also theres various other databases like ui_chara_db.prc, ui_clear_getter_db.prc, ui_spirit_db.prc, ui_howtoplay_db.prc, ui_item_db.prc, ui_popup_db.prc, ui_rule_db.prc and ui_stage_db.prc in the ui/param/database folder, and those arent all of them yet either, thats just how much I cracked into once I understood the base pattern.    

Note about the games music:  
Probably due to some bug, the music is only in 64kbps opus files, but the file container has the size of a 96kbps opus file, in fact if you use a hex editor to skip over the 64kbps stream, you can find the last 3rd of the music still in a higher quality 96kbps stream, which if you cut out the 64kbps stream and edit the opus header to now represent the smaller size can still be played back, not that its of much use, just wanted to point that out.  

Also if you want to compile it yourself, see the build.bat, it uses gcc and zstd (aka zstandard).  
