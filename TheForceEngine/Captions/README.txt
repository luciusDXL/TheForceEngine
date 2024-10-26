Each subtitle/caption is associated with an audio file by the name of the file. 
Captions are formatted like this:

    filename "Text of the caption" [duration]

The file name should include the `.voc` extension for gameplay sound effects but 
NOT for cutscene sound effects. The third parameter, duration, is an optional 
floating-point value indicating the duration of the caption in seconds (usually 
the same duration as the audio clip).

For example,

    m01moc03 "Dark Trooper: release." 1.5

This corresponds to General Moc's third line of dialog during the cutscene before 
Talay. Because this is for a cutscene, the file name (m01moc03) has no extension. 
The number at the end indicates that the subtitle should display for 1.5 seconds.
When the duration value is not included, the caption duration is automatically
calculated from the length of the caption (in bytes).

    ransto01.voc "Trooper: There he is, stop him!"

This line of voice is for a stormtrooper during gameplay. Note that the `.voc`
extension is included. No duration is specified, so the duration will be set
automatically based on the length of the text (the captioning system does not know
the length of the audio clip).

    door.voc 	"[Door hisses]"

This is a descriptive caption. Note the brackets around the text. The captioning
system automatically classifies any caption with brackets as a descriptive
caption. Descriptive captions can be enabled or disabled separately from voice
subtitles. Descriptive captions are supported in cutscenes and gameplay.

Be careful when adding new descriptive captions, as some sound effects play very
frequently and are obnoxious if captioned.

Lines that begin with // or # are ignored

    //this is a comment
    # this is also a comment

UTF-8 Unicode is supported, but make sure that you select a font which supports the
necessary character set. If a unicode character is missing from the selected font,
the missing character will be replaced by a rectangle or question mark.

Because automatic caption durations are calculated by measuring the length of the
caption in bytes, and UTF-8 characters may be more than 1 byte long, the automatic
duration may be too long when the caption consists primarily of multi-byte Unicode
characters. You can avoid this issue by including the duration value in the caption
file.