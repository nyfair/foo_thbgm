Usage:
Move foo_thbgm.dll to your foobar2000's components directory.
Move thxml file to your game's installation directory.
Simply open thxml files by foobar2000, then enjoy your life!

Something you should know:
Three new thbgm entries are added into foobar2000's playback menu.

Loop forever means BGM will always loop. Don't forget to disable loop forever mode when converting these BGMs.
Read metadata will use foobar2000 built-in splitter to read real BGM file's music tag instead of thxml. It's not a preset option since it will speed down unpack efficiency a lot.

The plugin can also extract files from 緋想天or非想天則or幡紫竜's file archives, just drag these packages files into foobar2000's main window
(e.g th123b.dat th105b.dat music.ac6), the extrated files will be created in a new folder under current path.

Some sound cards don't support play 神霊廟's ghost mode in Kernel Streaming or WASAPI becuase the frequency is 22050Hz.
You could add resampler in DSP Manager or change the output setting to default.

Feature:
1. A-B loop for any audio format.
2. Directly play bgm files of touhou and some related doujin games.

Supported Games
The Embodiment of Scarlet Devil (東方紅魔郷)
Perfect Cherry Blossom (東方妖々夢)
Imperishable Night (東方永夜抄)
Phantasmagoria of Flower View (東方花映塚)
Shoot the Bullet (東方文花帖)
Mountain of Faith (東方風神録)
Subterranean Animism (東方地霊殿)
Undefined Fantastic Object (東方星蓮船)
Double Spoiler (ダブルスポイラー～東方文花帖)
Fairy Wars (妖精大戦争～東方三月精)
Ten Desires (東方神霊廟)
Immaterial and Missing Power (東方萃夢想)
Scarlet Weather Rhapsody (東方緋想天)
Touhou Hisōtensoku (東方非想天則～超弩級ギニョルの謎を追え)
Uwabami Breakers (黄昏酒場)
Kioh Gyoku (稀翁玉)
Banshiryuu (幡紫竜) both c67 and later version
Samidare (五月雨)

ChangeLog:
1.1
add 神霊廟 support
add loop count setting
fix a bug that configuration can't be saved
1.0
add tasfro unpacker for 緋想天&非想天則
add 五月雨 support
fix 幡紫竜 thxml file
0.9
add ac6 unpacker for 幡紫竜c74 and later version
0.8
add common unpacker support
now we can play music in archive if only foobar2000 can handle it
0.5
add 幡紫竜c67 version support
add python3 script to convert THxxBGM files
0.4
add 萃夢想&黄昏酒場&稀翁玉 support
add 神霊廟trial ghost mode support
change tag file from cue to thxml
0.3
fix decode error in foobar2000 1.1.6
0.2
add 紅魔郷 support
0.1
initial release, support all touhou stg games from 妖々夢 to 神霊廟trial

For any feature request or bug report, feel free to contact me at my E-mail address below.

(C) nyfair <nyfair2012@gmail.com>
