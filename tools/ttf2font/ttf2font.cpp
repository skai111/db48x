// ****************************************************************************
//  ttf2font.cpp                                                 DB48X project
// ****************************************************************************
//
//   File Description:
//
//     This converts a TTF file to a DB48X font format
//
//     This is freely inspired by ttf2RasterFont in the wp43s project,
//     but simplified to generate the font data structures in DB48X
//
//
//
//
//
// ****************************************************************************
//   (C) 2022 Christophe de Dinechin <christophe@dinechin.org>
//   This software is licensed under the GNU General Public License v3
// ****************************************************************************
//   This file is part of DB48X.
//
//   DB48X is free software: you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation, either version 3 of the License, or
//   (at your option) any later version.
//
//   DB48X is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with DB48X.  If not, see <https://www.gnu.org/licenses/>.
// ****************************************************************************

#include <ctype.h>
#include <ft2build.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include FT_FREETYPE_H
#include <vector>
#include <cerrno>

typedef const char *cstring;
typedef unsigned char byte;
typedef unsigned uint;
typedef std::vector<byte> bytes;
typedef std::vector<int>  ints;

int verbose = 0;
int ascenderPct = 100;
int descenderPct = 100;
int heightPct = 100;
int yAdjustPixels = 0;


const char *getErrorMessage(FT_Error err)
// ----------------------------------------------------------------------------
//    Error messages from freetype2
// ----------------------------------------------------------------------------
{
#undef __FTERRORS_H__
#define FT_ERROR_START_LIST     switch (err) {
#define FT_ERRORDEF(e, v, s)       case e: return s;
#define FT_ERROR_END_LIST       }
#include FT_ERRORS_H
    return "(Unknown error)";
}


enum id
// ----------------------------------------------------------------------------
//    Get the IDs
// ----------------------------------------------------------------------------
{
#define ID(i)   ID_##i,
#include "ids.tbl"
};


int sortCharCodes(const void *a, const void *b)
// ----------------------------------------------------------------------------
//    Sort according to the char codes
// ----------------------------------------------------------------------------
{
    return *(FT_ULong *) a - *(FT_ULong *) b;
}


template<typename Data, typename Int = uint>
inline Data *leb128(Data *p, Int value)
// ----------------------------------------------------------------------------
//   Write the LEB value at pointer
// ----------------------------------------------------------------------------
{
    byte *bp = (byte *) p;
    do
    {
        *bp++ = (value & 0x7F) | 0x80;
        value = Int(value >> 7);
    } while (value != 0 && (Int(~0ULL) > Int(0) || value != Int(~0ULL)));
    bp[-1] &= ~0x80;
    return (Data *) bp;
}


template <typename Int>
void operator += (bytes &b, Int data)
// ----------------------------------------------------------------------------
//   Add some data in LEB128 form to a bytes vector
// ----------------------------------------------------------------------------
{
    byte buffer[16];
    byte *p = buffer;
    p = leb128(p, data);
    b.insert(b.end(), buffer, p);
}

template <typename T>
T *calloc(size_t sz)
// ----------------------------------------------------------------------------
//   Type-safe calloc
// ----------------------------------------------------------------------------
{
    return (T*) calloc(sz, sizeof(T));
}


void processFont(cstring fontName,
                 cstring ttfName,
                 cstring cSourceName,
                 int     fontSize,
                 int     threshold)
// ----------------------------------------------------------------------------
//   Process a font and generate the C source file from it
// ----------------------------------------------------------------------------
/* Font structure: There are two font formats, dense and sparse.
 * The dense format has a singe bitmap
 * The sparse format has one smaller bitmap per character
 * Both formats share the same offsets structure, and are packed using leb128.
 *
 * Dense format:
 * - ID_dense_font
 * - Total size of font data
 * - Height of bitmap
 * - Width of bitmap
 * - Bitmap data (Height * Width bits, padded to 8 bits)
 * - Sequence of ranges, each range being:
 *   + First code point in range, 0 at end of font data
 *   + Number of codepoints in range, 0 at end of font data
 *   + For each code point in range
 *     * Width of character in the bitmap
 * - Length of name
 * - Bytes for font name
 *
 * Sparse format:
 * - ID_sparse_font
 * - Total size of font data
 * - Height of characters
 * - Sequence of ranges, each range being:
 *   + First code point in range, 0 marks end of font data
 *   + Number of code points in range, 0 at end of font data
 *   + For each code point in range
 *     * X offset of character bitmap
 *     * Y offset of character bitmap
 *     * Width of character bitmap
 *     * Height of character bitmap
 *     * Advance to next character
 *     * Bitmap data for character, padded to 8 bits
 * - Length of name
 * - Bytes for font name
 *
 * The formats are designed to make it possible to store it efficiently as a
 * dynamic and moveable RPL object. The downside is that some linear scanning is
 * required when displaying characters. This is mitigated in the text display
 * code by keeping a cache for the current range, with the assumption that in
 * most cases, consecutive characters tend to be in the same range.
 *
 * The tool computes the total size of font data in either case, to let you pick
 * the representation that uses the least data.
 */
{
    // Open file before doing anything else, in case it fails
    FILE *output = fopen(cSourceName, "w");
    if (!output)
    {
        fprintf(stderr, "Cannot open source file %s", cSourceName);
        fprintf(stderr, "Error %d: %s", errno, strerror(errno));
        exit(1);
    }

    // Initialize freetype2
    FT_Library library;
    FT_Error   error = FT_Init_FreeType(&library);
    if (error != FT_Err_Ok)
    {
        fprintf(stderr, "Error during freetype2 library initialisation.\n");
        fprintf(stderr, "Error %d : %s\n", error, getErrorMessage(error));
        exit(1);
    }

    // Open the font face
    FT_Face   face;
    error = FT_New_Face(library, ttfName, 0, &face);
    if (error != FT_Err_Ok)
    {
        fprintf(stderr, "Error during face creation from file %s\n", ttfName);
        fprintf(stderr, "Error %d : %s\n", error, getErrorMessage(error));
        FT_Done_FreeType(library);
        exit(1);
    }

    // Set font size - Formula lifted from wp43s
    int unitsPerEM       = face->units_per_EM;
    int pixelSize        = unitsPerEM == 1024 ? 32 : 50;
    int baseSize         = unitsPerEM / pixelSize;
    int fontHeightPixels = fontSize ? fontSize : baseSize;
#define SCALED(x) ((x) * fontHeightPixels / baseSize)

    error = FT_Set_Pixel_Sizes(face, 0, fontHeightPixels);
    if (error != FT_Err_Ok)
    {
        fprintf(stderr, "Error setting pixel size from file %s\n", ttfName);
        fprintf(stderr, "Error %d : %s\n", error, getErrorMessage(error));
        FT_Done_FreeType(library);
        exit(1);
    }

    // Face scaling
    uint denseWidth  = 0;
    int  ascend      = face->ascender;
    int  descend     = face->descender;
    int  faceHeight  = ascend - descend;
    uint denseHeight = SCALED(faceHeight) / pixelSize;
    int  scAscend    = ascend * ascenderPct / 100;
    int  scDescend   = descend * descenderPct / 100;
    uint sparseHeight= SCALED(scAscend - scDescend) / pixelSize * heightPct/100;
    int  renderFlag  = FT_LOAD_RENDER;
    if (!threshold)
        renderFlag |= FT_LOAD_TARGET_MONO;

    // Count total number of glyphs
    uint      numberOfGlyphs = face->num_glyphs;
    FT_ULong *charCodes      = calloc<FT_ULong>(numberOfGlyphs);
    FT_ULong *curCharCode    = charCodes;
    FT_UInt   glyphIndex     = 0;
    uint      glyphCount     = 0;
    int       minRowsBelow   = 0;
    for (FT_ULong charCode = FT_Get_First_Char(face, &glyphIndex);
         glyphIndex;
         charCode          = FT_Get_Next_Char(face, charCode, &glyphIndex))
    {
        *curCharCode++      = charCode;
        error               = FT_Load_Glyph(face, glyphIndex, renderFlag);
        if (error != FT_Err_Ok)
        {
            fprintf(stderr, "warning: failed to load glyph 0x%04lX\n", charCode);
            fprintf(stderr, "Error %d : %s\n", error, getErrorMessage(error));
        }
        FT_Glyph_Metrics *m = &face->glyph->metrics;
        FT_Bitmap        *b = &face->glyph->bitmap;
        int rowsGlyph       = b->rows;
        int rowsDescend     = SCALED(descend) / pixelSize;
        int rowsBelowGlyph  = m->horiBearingY / 64 - rowsDescend - rowsGlyph;
        if (rowsBelowGlyph < minRowsBelow)
            minRowsBelow = rowsBelowGlyph;

        denseWidth += face->glyph->metrics.horiAdvance / 64;
        glyphCount++;
    }

    if (verbose || glyphCount > numberOfGlyphs)
    {
        fprintf(stderr, "Number of glyphs %u, glyph count %u, width %u\n",
                numberOfGlyphs, glyphCount, denseWidth);
        if (glyphCount > numberOfGlyphs)
            exit(1);
    }

    // Sort the glyphs in increasing Unicode codepoint order
    qsort(charCodes, glyphCount, sizeof(charCodes[0]), sortCharCodes);

    // Adjustment of bitmap height if there are descenders below
    if (minRowsBelow < 0)
        denseHeight -= minRowsBelow;

    // Round up the bitmap Width to full bytes
    denseWidth = (denseWidth + 7) / 8 * 8;

    // Dense format data
    // Allocate the global bitmap where we will draw glyphs and other structures
    size_t       denseBitMapSize = (denseWidth * denseHeight + 7) / 8;
    byte        *denseBitMap     = calloc<byte>(denseBitMapSize);
    bytes        dense;

    // Sparse format data
    bytes        sparse;
    sparse += sparseHeight;

    if (verbose)
        printf("Font bitmap width %u height %u size %u\n",
               denseWidth, denseHeight, (uint) denseBitMapSize);

    // Set the transform for the font
    FT_Vector pen;
    pen.x = 0;
    pen.y = (fontHeightPixels - SCALED(face->ascender) / pixelSize) * 64;
    FT_Set_Transform(face, NULL, &pen);

    // Start on the left of the dense bitmap
    int32_t   denseBitMapX = 0;
    FT_ULong  firstCode    = 0;
    FT_ULong  currentCode  = 0;

    // Find the ranges in the font
    ints     rangesFirst;
    ints     rangesCount;

    // Loop on all glyphs
    for (uint g = 0; g < glyphCount; g++)
    {
        FT_ULong charCode   = charCodes[g];
        FT_UInt  glyphIndex = FT_Get_Char_Index(face, charCode);
        if (glyphIndex == 0)
        {
            fprintf(stderr, "Glyph 0x%04lX undefined\n", charCode);
            continue;
        }

        // Check if we begin a new glyph range
        if (charCode != currentCode || g+1 == glyphCount)
        {
            uint32_t numCodes = currentCode - firstCode;
            if (numCodes)
            {
                if (verbose)
                    printf("New glyph range at %u, had %u codes in %lu..%lu\n",
                           (int) charCode, numCodes, firstCode, currentCode);
                rangesFirst.push_back(firstCode);
                rangesCount.push_back(numCodes);
            }
            currentCode = charCode;
            firstCode = charCode;
        }
        currentCode++;
    }

    if (verbose)
        printf("Found %zu glyph ranges\n", rangesFirst.size());

    // Loop on all ranges
    uint numRanges = rangesFirst.size();
    uint glyph = 0;
    for (uint r = 0; r < numRanges; r++)
    {
        uint firstCode = rangesFirst[r];
        uint numCodes = rangesCount[r];
        uint lastCode = firstCode + numCodes;

        // Write out header for the range
        dense += firstCode;
        dense += numCodes;
        sparse += firstCode;
        sparse += numCodes;

        // Loop on all glyphs in range
        for (uint g = firstCode; g < lastCode; g++)
        {
            FT_ULong charCode   = charCodes[glyph++];
            FT_UInt  glyphIndex = FT_Get_Char_Index(face, charCode);
            if (glyphIndex == 0)
                continue;

            error = FT_Load_Glyph(face, glyphIndex, renderFlag);
            if (error != FT_Err_Ok)
            {
                fprintf(stderr,
                        "Warning: failed to load glyph 0x%04lX\n", charCode);
                fprintf(stderr,
                        "Error %d : %s\n", error, getErrorMessage(error));
                continue;
            }

            // Get glyph metrics and bitmap
            FT_Glyph_Metrics *m = &face->glyph->metrics;
            FT_Bitmap        *b = &face->glyph->bitmap;

            // Columns in the glyph
            int glyphWidth      = m->horiAdvance / 64;
            int colsBeforeGlyph = m->horiBearingX / 64;
            int colsGlyph       = b->width;
            int colsRight       = colsBeforeGlyph + colsGlyph;
            int colsAfterGlyph  = glyphWidth - colsRight;

            // Rows in the glyph
            int rowsAscend     = SCALED(ascend) / pixelSize;
            int rowsAboveGlyph = rowsAscend - m->horiBearingY / 64;
            int rowsAboveSave  = rowsAboveGlyph;
            int rowsGlyph      = b->rows;
            int rowsDescend    = SCALED(descend) / pixelSize;
            int rowsBelowGlyph = m->horiBearingY / 64 - rowsDescend - rowsGlyph;
            int rowsBelowSave  = rowsBelowGlyph;

            // Adjust positions for dense bitmaps
            if (colsBeforeGlyph < 0)
            {
                colsGlyph += colsBeforeGlyph;
                colsBeforeGlyph = 0;
            }
            if (false && rowsAboveGlyph < 0)
            {
                rowsGlyph += rowsAboveGlyph;
                rowsAboveGlyph = 0;
            }

            // Fill sparse data header
            sparse += colsBeforeGlyph;
            sparse += rowsAboveGlyph * ascenderPct / 100 + yAdjustPixels;
            sparse += colsGlyph;
            sparse += rowsGlyph;
            sparse += glyphWidth;

            // Allocate the sparse bitmap
            uint sparseBitmapBits = colsGlyph * rowsGlyph;
            uint sparseBitmapBytes = (sparseBitmapBits + 7) / 8;
            bytes sparseBits;
            sparseBits.resize(sparseBitmapBytes);

            // Fill sparse and dense bitmaps from glyph bitmap
            byte *buffer = face->glyph->bitmap.buffer;
            uint  pitch  = face->glyph->bitmap.pitch;
            uint  bwidth = face->glyph->bitmap.width;
            uint  rwidth = colsGlyph - 1;
            for (int y = 0; y < rowsGlyph; y++)
            {
                int by = y + rowsAboveGlyph;
                if (by < 0)
                    continue;
                for (int x = 0; x < colsGlyph; x++)
                {
                    int bit = 0;
                    if (threshold)
                    {
                        int bo = y * bwidth + x;
                        bit = buffer[bo] >= threshold;
                    }
                    else
                    {
                        int bo = y * pitch + x/8;
                        bit = (buffer[bo] >> (7 - x % 8)) & 1;
                    }
                    if (verbose)
                        putchar(bit ? '#' : '.');

                    int dbo = y * colsGlyph + (rwidth - x);
                    if (bit)
                    {
                        int bx = denseBitMapX + x + colsBeforeGlyph;
                        uint32_t bitOffset = by * denseWidth + bx;
                        uint32_t byteOffset = bitOffset / 8;
                        if (byteOffset > denseBitMapSize)
                        {
                            fprintf(stderr, "Ooops, wordOffset=%u, size=%u\n"
                                    "  bx=%u by=%u bitOffset=%u\n",
                                    byteOffset, (uint) denseBitMapSize,
                                    bx, by, bitOffset);
                            exit(127);
                        }
                        denseBitMap[byteOffset] |= 1 << (bitOffset % 8);
                        sparseBits[dbo / 8] |= 1 << (dbo % 8);
                    }
                }
                if (verbose)
                    putchar('\n');
            }

            // Add sparse bitmap to sparse data
            sparse.insert(sparse.end(), sparseBits.begin(), sparseBits.end());

            // Add X coordinate in dense bitmap to dense data
            dense += glyphWidth;
            denseBitMapX += glyphWidth;

            // Verbose output about that glyph
            if (verbose)
            {
                char utf8[4] = { 0 };
                if (charCode < 0x80)
                {
                    utf8[0] = charCode;
                }
                else if (charCode < 0x800)
                {
                    utf8[0] = 0xC0 | (charCode >> 6);
                    utf8[1] = 0x80 | (charCode & 63);
                }
                else if (charCode < 0x10000)
                {
                    utf8[0] = 0xE0 | (charCode >> 12);
                    utf8[1] = 0x80 | ((charCode >> 6) & 63);
                    utf8[2] = 0x80 | ((charCode >> 0) & 63);
                }
                else
                {
                    utf8[0] = utf8[1] = utf8[2] = '-';
                }
                printf("Glyph %4lu '%s' width %u"
                       "  Columns: %d %d %d"
                       "  Rows: %d %d %d\n",
                       charCode,
                       utf8,
                       glyphWidth,
                       colsBeforeGlyph,
                       colsGlyph,
                       colsAfterGlyph,
                       rowsAboveSave,
                       rowsGlyph,
                       rowsBelowSave);
                printf("    advance:%ld hBearingX:%ld horiBearingY:%ld\n",
                       m->horiAdvance, m->horiBearingX, m->horiBearingY);
            } // Verbose output
        } // Loop on codes
    } // Loop on ranges

    // Insert terminating 0 code point to mark end of ranges
    dense += 0;
    dense += 0;
    sparse += 0;
    sparse += 0;

    // Add the length of the name followed by the name itself
    size_t nameLen = strlen(fontName);
    dense += nameLen;
    sparse += nameLen;
    for (uint i = 0; i < nameLen; i++)
    {
        dense.push_back((byte) fontName[i]);
        sparse.push_back((byte) fontName[i]);
    }

    // Insert bitmap data at beginning of dense data
    dense.insert(dense.begin(), denseBitMap, denseBitMap + denseBitMapSize);
    bytes denseInfo;
    denseInfo += denseHeight;
    denseInfo += denseWidth;
    dense.insert(dense.begin(), denseInfo.begin(), denseInfo.end());

    // Emit the headers
    bytes sparseHeader;
    sparseHeader += unsigned(ID_sparse_font);
    if (sparseHeader.size() != 2)
    {
        fprintf(stderr,
                "Invalid sparse header ID size %zu\n", sparseHeader.size());
        exit(1);
    }
    sparseHeader += sparse.size();
    sparse.insert(sparse.begin(), sparseHeader.begin(), sparseHeader.end());

    bytes denseHeader;
    denseHeader += unsigned(ID_dense_font);
    if (denseHeader.size() != 2)
    {
        fprintf(stderr,
                "Invalid dense header ID size %zu\n", denseHeader.size());
        exit(1);
    }
    denseHeader += dense.size();
    dense.insert(dense.begin(), denseHeader.begin(), denseHeader.end());

    size_t denseSize = dense.size();
    size_t sparseSize = sparse.size();
    if (verbose)
        printf("Sizes: dense %zu, sparse %zu\n", denseSize, sparseSize);


    // Now time to emit the actual data
    fprintf(output,
            "/** Font %s, generated from %s - Do not edit manually **/\n"
            "\n"
            "#include \"font.h\"\n"
            "#include \"target.h\"\n"
            "#include \"object.h\"\n"
            "\n",
            fontName, ttfName);

    if (denseSize < sparseSize || verbose)
    {
        fprintf(output,
                "extern const unsigned char %s_dense_font_data[];\n",
                fontName);
        fprintf(output,
                "const unsigned char %s_dense_font_data[%zu] FONT_QSPI =\n"
                "{\n",
                fontName, denseSize);

        fprintf(output,
                "    "
                "byte(object::ID_dense_font) | 0x80, "
                "byte(uint(object::ID_dense_font) >> 7), ");

        for (uint b = 2; b < denseSize; b++)
            fprintf(output, "%s0x%02X,",
                    b % 16 == 0 ? "\n    " : " ",
                    dense[b]);
        fprintf(output, "\n};\n");
    }

    if (sparseSize <= denseSize || verbose)
    {
        fprintf(output,
                "extern const unsigned char %s_sparse_font_data[];\n",
                fontName);
        fprintf(output,
                "const unsigned char %s_sparse_font_data[%zu] FONT_QSPI =\n"
                "{\n",
                fontName, sparseSize);

        fprintf(output,
                "    "
                "byte(object::ID_sparse_font) | 0x80, "
                "byte(uint(object::ID_sparse_font) >> 7), ");

        for (uint b = 2; b < sparseSize; b++)
            fprintf(output, "%s0x%02X,",
                    b % 16 == 0 ? "\n    " : " ",
                    sparse[b]);
        fprintf(output, "\n};\n");
    }

    fclose(output);

    // Free memory
    free(denseBitMap);
    free(charCodes);

    // Free the face and library ressources
    FT_Done_Face(face);
    FT_Done_FreeType(library);
}


void usage(cstring prog)
// ----------------------------------------------------------------------------
//   Display the usage message for the program
// ----------------------------------------------------------------------------
{
    printf("Usage: %s [-h] [-v] [-s <size>] <name> <ttf> <output>\n"
           "  name: Name of the structure in C\n"
           "  ttf: TrueType input font\n"
           "  output: C source file to be generated\n"
           "  -h: Display this usage message\n"
           "  -a: Adjust ascender (percentage)\n"
           "  -d: Adjust descender (percentage)\n"
           "  -s <size>: Force font size to s pixels\n"
           "  -v: Verbose output\n"
           "  -y: Adjust Y position",
           prog);
}


int main(int argc, char *argv[])
// ----------------------------------------------------------------------------
//   Run the tool
// ----------------------------------------------------------------------------
{
    // Process options
    int opt;
    int fontSize = 0;
    int threshold = 0;
    while ((opt = getopt(argc, argv, "a:d:hs:S:t:vy:")) != -1)
    {
        switch (opt)
        {
        case 'a':
            ascenderPct = atoi(optarg);
            break;
        case 'd':
            descenderPct = atoi(optarg);
            break;
        case 'v':
            verbose = 1;
            break;
        case 's':
            fontSize = atoi(optarg);
            break;
        case 'S':
            heightPct = atoi(optarg);
            break;
        case 't':
            threshold = atoi(optarg);
            break;
        case 'y':
            yAdjustPixels = atoi(optarg);
            printf("Adjust pixels = %d\n", yAdjustPixels);
            break;
        case 'h':
            usage(argv[0]);
            exit(0);
        default:
            usage(argv[0]);
            exit(1);
        }
    }
    argc -= optind;
    if (argc < 3)
    {
        usage(argv[0]);
        return 1;
    }
    argv += optind;

    // Generate the C source code
    processFont(argv[0], argv[1], argv[2], fontSize, threshold);

    return 0;
}
