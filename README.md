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


Also if you want to compile it yourself, see the build.bat, it uses gcc and zstd.  
