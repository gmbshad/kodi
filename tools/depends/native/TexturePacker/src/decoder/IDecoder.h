/*
 *      Copyright (C) 2014 Team Kodi
 *      http://kodi.tv
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#pragma once

#include <string>
#include <vector>

/* forward declarations */

class DecodedFrame;
class DecodedFrames;

class IDecoder
{
  public:
    virtual ~IDecoder() = default;
    virtual bool CanDecode(const std::string &filename) = 0;
    virtual bool LoadFile(const std::string &filename, DecodedFrames &frames) = 0;
    virtual void FreeDecodedFrame(DecodedFrame &frame) = 0;
    virtual const char* GetImageFormatName() = 0;
    virtual const char* GetDecoderName() = 0;

    const std::vector<std::string>& GetSupportedExtensions()
    {
      m_supportedExtensions.clear();
      FillSupportedExtensions();
      return m_supportedExtensions;
    }

  protected:
    virtual void FillSupportedExtensions() = 0;
    //fill this with extensions in FillSupportedExtensions like ".png"
    std::vector<std::string> m_supportedExtensions;
};

class RGBAImage
{
public:
  RGBAImage() : pixels(NULL), width(0), height(0), bbp(0), pitch(0) {}

  char *pixels;//image data
  int width;// width
  int height;// height
  int bbp;// bits per pixel
  int pitch;// rowsize in bytes
};

class DecodedFrame
{
  public:
   DecodedFrame() : delay(0) { }
   RGBAImage rgbaImage; /* rgbaimage for this frame */
   int delay = 0; /* Frame delay in ms */
   IDecoder* decoder = nullptr; /* Pointer to decoder */
};

class DecodedFrames
{
  public:
    DecodedFrames() = default;
    std::vector<DecodedFrame> frameList;

    void clear()
    {
      for (auto f : frameList)
      {
        if (f.decoder != NULL)
        {
          f.decoder->FreeDecodedFrame(f);
        }
        else
        {
          fprintf(stderr,
            "ERROR: %s - can not determine decoder type for frame!\n",
            __FUNCTION__);
        }
      }
      frameList.clear();
    }
};
