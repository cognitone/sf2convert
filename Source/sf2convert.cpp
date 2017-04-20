///////////////////////////////////////////////////////////////////////////////
//
//  sf2convert
//  SoundFont Conversion/Compression Utility
//
//  Copyright (C)
//  2010 Werner Schweer and others (MuseScore)
//  2015 Davy Triponney (Polyphone)
//  2017 Cognitone (Juce port, converter)
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//
///////////////////////////////////////////////////////////////////////////////

#include "../JuceLibraryCode/JuceHeader.h"
#include "sfont.h"

//---------------------------------------------------------
//   usage
//---------------------------------------------------------

static void usage(const char* pname)
{
    fprintf(stderr, "sf2convert - SoundFont Compression Utility, 2017 Cognitone\n");
    fprintf(stderr, "usage: %s [-flags] infile outfile\n", pname);
    fprintf(stderr, "flags:\n");
    fprintf(stderr, "   -zf    compress source file using FLAC (SF4 format)\n");
    fprintf(stderr, "   -zf0   ditto w/quality=low\n");
    fprintf(stderr, "   -zf1   ditto w/quality=medium\n");
    fprintf(stderr, "   -zf2   ditto w/quality=high (default)\n");
    
    fprintf(stderr, "   -zo    compress source file using Ogg Vorbis (SF3 format)\n");
    fprintf(stderr, "   -zo0   ditto w/quality=low\n");
    fprintf(stderr, "   -zo1   ditto w/quality=medium\n");
    fprintf(stderr, "   -zo2   ditto w/quality=high (default)\n");
    
    fprintf(stderr, "   -x     expand source file to SF2 format\n");
    fprintf(stderr, "   -d     dump presets\n");
}

//---------------------------------------------------------
//   main
//---------------------------------------------------------

int main(int argc, char* argv[])
{
    bool dump = false;
    bool convert = false;
    SF2::FileType format = SF2::SF2Format;
    int  quality = 2;
    bool any = false;
    
    StringArray commandLine (argv + 1, argc - 1);
    /** Lacking getopt() on Windows, this is a quick & simple hack to pasre command line options */
    while (commandLine.size() > 2)
    {
        String token = commandLine.getReference(0);
        if (token.startsWith("-"))
        {
            if (token.indexOfChar('x') > 0)
            {
                convert = true;
                format = SF2::SF2Format;
                any = true;
            }
            if (token.indexOfChar('z') > 0)
            {
                convert = true;
                format = SF2::SF3Format;
                any = true;
            }
            if (token.indexOfChar('o') > 0)
            {
                convert = true;
                format = SF2::SF3Format;
                any = true;
            }
            if (token.indexOfChar('f') > 0)
            {
                convert = true;
                format = SF2::SF4Format;
                any = true;
            }
            if (token.indexOfChar('d') > 0)
            {
                dump = true;
                any = true;
            }
            if (token.indexOfChar('0') > 0)
            {
                quality = 0;
            }
            if (token.indexOfChar('1') > 0)
            {
                quality = 1;
            }
            if (token.indexOfChar('2') > 0)
            {
                quality = 2;
            }
            //DBG(token);
            commandLine.remove(0);
        }
    }
    
    const char* pname = argv[0];

    if (commandLine.size() != 2 && !any)
    {
        usage(pname);
        exit(1);
    }
    
    File inFilename (commandLine[0]);
    File outFilename (commandLine[1]);

    {
        SF2::SoundFont sf(inFilename);
        sf.log("Reading " + inFilename.getFullPathName());
        
        if (!sf.read()) {
            fprintf(stderr, "Error reading file\n");
            return(3);
        }
        
        if (dump)
            sf.dumpPresets();

        if (convert)
        {
            sf.log("Writing " + outFilename.getFullPathName());
            sf.write (outFilename, format, quality);
        }
    }
    return 0;
}

