//=============================================================================
//  sf2convert
//  SoundFont Conversion/Compression Utility
//
//  Copyright (C) 2015 Davy Triponney (Polyphone)
//                2010 Werner Schweer and others (MuseScore)
//                2017 Cognitone
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
//=============================================================================

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
    
    int c;
    bool any = false;
    while ((c = getopt(argc, argv, "xzofd012")) != EOF) {
        switch(c) {
                
            case 'x':
                convert = true;
                format = SF2::SF2Format;
                any = true;
                break;
                
            case 'z':
                convert = true;
                format = SF2::SF3Format;
                any = true;
                break;
                
            case 'o':
                convert = true;
                format = SF2::SF3Format;
                any = true;
                break;
                
            case 'f':
                convert = true;
                format = SF2::SF4Format;
                any = true;
                break;
                
            case 'd':
                dump = true;
                any = true;
                break;
                
            case '0':
                quality = 0;
                break;
            case '1':
                quality = 1;
                break;
            case '2':
                quality = 2;
                break;
                
            default:
                usage(argv[0]);
                exit(1);
        }
    }
    const char* pname = argv[0];

    if (argc < 3) {
        usage(pname);
        exit(1);
    }
    
    argc -= 1;
    argv += 1;
    if (!any) {
        usage(pname);
        exit(2);
    }
    
    File inFilename (argv[1]);
    File outFilename (argv[2]);
    
    SF2::SoundFont sf(inFilename);
    sf.log("Reading " + inFilename.getFullPathName());
    
    if (!sf.read()) {
        fprintf(stderr, "file read error\n");
        exit(3);
    }
    
    if (dump)
        sf.dumpPresets();

    if (convert)
    {
        sf.log("Writing " + outFilename.getFullPathName());
        sf.write (outFilename, format, quality);
    }
    return 0;
}

