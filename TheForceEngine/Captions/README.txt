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