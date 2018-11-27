# ssbu-av-extract  
Aims to extract various files from the ssbu data.arc.  
Right now only music/video files get extracted (first 2gb of the file), though I have started looking into the rest of the file a little, I may work on it some more.  
See the "releases" tab for a built version of that, just put it into the same folder as the data.arc, run it and wait for it to output music into a "lopus" and video into a "webm" folder.  
The .lopus music files can be played back with any player that has the vgmstream plugin and the videos can be played back in any video player, I tried mpv for this and it works fine.  
Some of the video files also contain multiple audio language tracks that you can switch to in your video player.    

Also if you want to compile it yourself, see the build.bat, it just uses gcc.  
