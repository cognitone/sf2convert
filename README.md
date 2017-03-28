# sf2convert
### Convert SF2 SoundFonts to SF3 (Vorbis) or SF4 (FLAC) and back 

This utility converts SF2 files to SF3 format, using Ogg Vorbis for sample compression (SF3 is natively supported by MuseScore). Conversion of SF3 back to SF2 is also possible, however since Ogg Vorbis is lossy, SF3 is not useful for archiving purposes.

Alternatively, sf2convert converts to a new proposed SF4 format (not a standard in any way yet), using lossless FLAC for sample compression. This format is well suited for archiving and maybe a workable replacement for the legacy sfArk format.

Based on the SoundFont reader/writer class by Davy Triponney (2015, [Polphone](https://github.com/davy7125/polyphone)), Werner Schweer and others (2010, [MuseScore](https://github.com/musescore/MuseScore)), which was ported from Qt to the [Juce framework](https://www.juce.com) and somewhat extended in order allow for easy integration with Juce-based audio projects. It makes use of the Vorbis and FLAC classes of Juce. In order to compile sf2convert, you'll' need Juce version 4. As a result, sf2convert compiles and runs on Mac, Windows, Linux and more.

This repository does not include compiled binaries. If you want to build, host and distribute compiled binaries on your site for the convenience of non-developers, this is very welcome.

## Usage

Compression with Ogg Vorbis (o)    
`sf2convert -zo <infile.SF2> <outfile.SF3>`    
     
Compression with FLAC (f)    
`sf2convert -zf <infile.SF2> <outfile.SF4>`    
    
Extraction of any compressed format:    
`sf2convert -x <infile.SF3> <outfile.SF2>    
sf2convert -x <infile.SF4> <outfile.SF2>`    
    
For additional options, run the utility with an empty command line.


## Notes

The SoundFont class has only been ported to an extent that makes it work for one shot use, i.e. read and write one file and then quit. There's no proper destructor yet that handles all its components. Also, it might be desirable to use Juce Strings instead of plain C char pointers. This has not yet been a priority. So if you want to use this class in your Juce project in its current state, that's what you'll have to look at first.

Released by Cognitone under the GPL license.
