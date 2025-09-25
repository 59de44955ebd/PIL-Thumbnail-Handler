#include <windows.h>
#include <iostream>
#include <zlib.h>

#define MODE(A, B, C, D) (unsigned long)(A | (B << 8) | (C << 16) | (D << 24))

#define CLIP8(v) ((v) <= 0 ? 0 : (v) < 256 ? (v) : 255)

#define SHIFTFORDIV255(a) ((((a) >> 8) + a) >> 8)

/* like (a * b + 127) / 255), but much faster on most platforms */
#define MULDIV255(a, b, tmp) (tmp = (a) * (b) + 128, SHIFTFORDIV255(tmp))

extern "C"
void ImagingConvertYCbCr2BGR(UINT8 *out, const UINT8 *in, int pixels);

void
cmyk2bgrx(UINT8 *out, const UINT8 *in, int xsize)
{
	int x, nk, tmp;
	for (x = 0; x < xsize; x++)
	{
		nk = 255 - in[3];
		out[0] = CLIP8(nk - MULDIV255(in[2], nk, tmp));
		out[1] = CLIP8(nk - MULDIV255(in[1], nk, tmp));
		out[2] = CLIP8(nk - MULDIV255(in[0], nk, tmp));
		out[3] = 255;
		out += 4;
		in += 4;
	}
}

#define FLOAT32 float

static void
f2i(UINT8 *out_, const UINT8 *in_, int xsize)
{
	int x;
	for (x = 0; x < xsize; x++, in_ += 4, out_ += 4)
	{
		FLOAT32 f;
		INT32 i;
		memcpy(&f, in_, sizeof(f));
		i = (INT32)f;
		memcpy(out_, &i, sizeof(i));
	}
}

static void
i2rgb(UINT8 *out, const UINT8 *in_, int xsize)
{
	int x;
	INT32 *in = (INT32 *)in_;
	for (x = 0; x < xsize; x++, in++, out += 4)
	{
		if (*in <= 0)
		{
			out[0] = out[1] = out[2] = 0;
		}
		else if (*in >= 255)
		{
			out[0] = out[1] = out[2] = 255;
		}
		else
		{
			out[0] = out[1] = out[2] = (UINT8)*in;
		}
		out[3] = 255;
	}
}

static void
hsv2bgr(UINT8 *out, const UINT8 *in, int xsize)
{  // following colorsys.py

	int p, q, t;
	UINT8 up, uq, ut;
	int i, x;
	float f, fs;
	UINT8 h, s, v;

	for (x = 0; x < xsize; x++, in += 3)
	{
		h = in[0];
		s = in[1];
		v = in[2];

		if (s == 0)
		{
			*out++ = v;
			*out++ = v;
			*out++ = v;
		}
		else
		{
			i = (int)floor((float)h * 6.0f / 255.0f); // 0 - 6
			f = (float)h * 6.0f / 255.0f - (float)i; // 0-1 : remainder.
			fs = ((float)s) / 255.0f;

			p = (int)round((float)v * (1.0f - fs));
			q = (int)round((float)v * (1.0f - fs * f));
			t = (int)round((float)v * (1.0f - fs * (1.0f - f)));
			up = (UINT8)CLIP8(p);
			uq = (UINT8)CLIP8(q);
			ut = (UINT8)CLIP8(t);

			switch (i % 6)
			{
			case 0:
				*out++ = up;
				*out++ = ut;
				*out++ = v;
				break;
			case 1:
				*out++ = up;
				*out++ = v;
				*out++ = uq;
				break;
			case 2:
				*out++ = ut;
				*out++ = v;
				*out++ = up;
				break;
			case 3:
				*out++ = v;
				*out++ = uq;
				*out++ = up;
				break;
			case 4:
				*out++ = v;
				*out++ = up;
				*out++ = ut;
				break;
			case 5:
				*out++ = uq;
				*out++ = up;
				*out++ = v;
				break;
			}
		}
		//*out++ = in[3];
	}
}

////////////////////////////////////////
// 
////////////////////////////////////////
BYTE * InflateData(const BYTE *input, int in_len, int *out_len, int raw = 0)
{
	BYTE *output;
	z_stream d_stream; // decompression stream
	int err;
	int windowBits;

	windowBits = raw ? -15 : 15;

	output = (BYTE*)malloc(*out_len + 1); // ???
	memset(output, 0, *out_len + 1);

	d_stream.zalloc = (alloc_func)0;
	d_stream.zfree = (free_func)0;
	d_stream.opaque = (voidpf)0;

	d_stream.next_in = (Bytef*)input;
	d_stream.avail_in = 0; //0; ???
	d_stream.next_out = output;

	//err = inflateInit(&d_stream);
	err = inflateInit2(&d_stream, windowBits);

	//CHECK_ERR(err, "inflateInit");
	if (err != Z_OK)
	{
		free(output);
		return NULL;
	}

	while (1)
	{
		d_stream.avail_in = d_stream.avail_out = 1; // force small buffers
		err = inflate(&d_stream, Z_NO_FLUSH);
		if (err == Z_STREAM_END) break;
		else if (err != Z_OK)
		{
			free(output);
			return NULL;
		}
		//CHECK_ERR(err, "inflate");
	}

	*out_len = d_stream.total_out;

	err = inflateEnd(&d_stream);
	//CHECK_ERR(err, "inflateEnd");
	if (err != Z_OK)
	{
		free(output);
		return NULL;
	}

	return output;
}

////////////////////////////////////////
// Saves HBITMAP as BMP file.
// HBITMAP can be 1, 4, 8, 16, 24 or 32 bit.
////////////////////////////////////////
BOOL SaveBitmap(HBITMAP h_bitmap, const wchar_t* bmp_file)
{
	BITMAP bm = {};
	GetObjectW(h_bitmap, sizeof(BITMAP), &bm);

	DWORD biClrUsed = bm.bmBitsPixel <= 8 ? 1 << bm.bmBitsPixel : 0;
	DWORD pal_size = 4 * biClrUsed;

	BYTE* bih = (BYTE*)malloc(sizeof(BITMAPINFOHEADER) + 4 * biClrUsed);

	BITMAPINFOHEADER *bmiHeader = (BITMAPINFOHEADER*)bih;
	bmiHeader->biSize = sizeof(BITMAPINFOHEADER);
	bmiHeader->biWidth = bm.bmWidth;
	bmiHeader->biHeight = -bm.bmHeight;
	bmiHeader->biPlanes = bm.bmPlanes;
	bmiHeader->biBitCount = bm.bmBitsPixel;
	bmiHeader->biCompression = BI_RGB;
	bmiHeader->biSizeImage = ((((bm.bmWidth * bmiHeader->biBitCount) + 31) & ~31) >> 3) * bm.bmHeight;
	bmiHeader->biXPelsPerMeter = 0;
	bmiHeader->biYPelsPerMeter = 0;
	bmiHeader->biClrUsed = biClrUsed;
	bmiHeader->biClrImportant = 0;

	// Create BITMAPFILEHEADER
	BITMAPFILEHEADER bfh = {};
	bfh.bfType = 0x4D42;
	bfh.bfSize = sizeof(BITMAPFILEHEADER) + bmiHeader->biSize + bmiHeader->biSizeImage + pal_size;
	bfh.bfReserved1 = 0;
	bfh.bfReserved2 = 0;
	bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + bmiHeader->biSize + pal_size;

	// Create buffer and read the actual pixel data into it
	BYTE* bits = (BYTE*)malloc(bmiHeader->biSizeImage);
	HDC hdc = CreateCompatibleDC(NULL);
	SelectObject(hdc, h_bitmap);
	BITMAPINFO *pbmi = (BITMAPINFO *)bih;
	GetDIBits(hdc, h_bitmap, 0, bm.bmHeight, bits, pbmi, DIB_RGB_COLORS);
	DeleteDC(hdc);

	// Create and open new BMP file
	HANDLE fh_bmp = CreateFile(
		bmp_file, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL
	);
	if (fh_bmp == INVALID_HANDLE_VALUE)
		return FALSE;

	// Write headers and data to file
	DWORD dwWritten;
	WriteFile(fh_bmp, (LPVOID)&bfh, sizeof(BITMAPFILEHEADER), &dwWritten, NULL);
	WriteFile(fh_bmp, bih, sizeof(BITMAPINFOHEADER) + 4 * biClrUsed, &dwWritten, NULL);
	WriteFile(fh_bmp, (LPVOID)bits, bmiHeader->biSizeImage, &dwWritten, NULL);

	free(bits);
	free(bih);

	// Close file handle
	CloseHandle(fh_bmp);

	return TRUE;
}

////////////////////////////////////////
//
////////////////////////////////////////
HBITMAP CreateMask(HDC hDC, HBITMAP hSrcBmp, COLORREF crTransparent, int width, int height)
{
	// create the source and mask dcs
	HDC hSrcDC = CreateCompatibleDC(hDC);
	HDC hMaskDC = CreateCompatibleDC(hDC);

	// create the mask
	HBITMAP hMaskBmp = CreateBitmap(width, height, 1, 1, 0);

	// select the source and mask bmps
	SelectObject(hSrcDC, hSrcBmp);
	SelectObject(hMaskDC, hMaskBmp);

	// create mask
	//nOldBkColor = 
	SetBkColor(hSrcDC, crTransparent);
	BitBlt(hMaskDC, 0, 0, width, height, hSrcDC, 0, 0, NOTSRCCOPY);
	//SetBkColor(hSrcDC, nOldBkColor);

	// Delete the helper DC
	DeleteDC(hMaskDC);
	DeleteDC(hSrcDC);

	return hMaskBmp;
}

////////////////////////////////////////
//
////////////////////////////////////////
void SetAlpha(HDC hdc, int width, int height, BYTE alpha)
{
	RGBQUAD fullAlpha = { 0x00, 0x00, 0x00, alpha };
	BITMAPINFO bi = {};
	bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bi.bmiHeader.biWidth = 1;
	bi.bmiHeader.biHeight = 1;
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 32;
	bi.bmiHeader.biCompression = BI_RGB;
	StretchDIBits(
		hdc, 0, 0, width, height,
		0, 0, 1, 1, &fullAlpha, &bi,
		DIB_RGB_COLORS,
		SRCPAINT
	);
}

////////////////////////////////////////
//
////////////////////////////////////////
HBITMAP ResizeBitmap(HBITMAP hbm, int destWidth, int destHeight, BOOL useAlpha, BOOL hasTransparency, COLORREF crTransparent, RGBQUAD * alphaQuads)
{
	BITMAP bm = {};
	GetObjectW(hbm, sizeof(BITMAP), &bm);

	if (alphaQuads) // PA or LA
	{
		HDC hdc = GetDC(NULL);

		////////////////////////////////////////
		// Convert from 8 bit to 32 bit
		////////////////////////////////////////

		HDC hdcSrc = CreateCompatibleDC(hdc);
		SelectObject(hdcSrc, hbm);

		HDC hdc32 = CreateCompatibleDC(hdc);
		HBITMAP hbm32 = CreateCompatibleBitmap(hdc, bm.bmWidth, bm.bmHeight);
		SelectObject(hdc32, hbm32);

		BitBlt(
			// dest
			hdc32, 0, 0, bm.bmWidth, bm.bmHeight,
			// scr
			hdcSrc, 0, 0,
			SRCCOPY
		);

		DeleteDC(hdcSrc);

		////////////////////////////////////////
		// Set the alpha channel
		////////////////////////////////////////

		BITMAPINFO bi = {};
		bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bi.bmiHeader.biWidth = bm.bmWidth;
		bi.bmiHeader.biHeight = bm.bmHeight;
		bi.bmiHeader.biPlanes = 1;
		bi.bmiHeader.biBitCount = 32;
		bi.bmiHeader.biCompression = BI_RGB;

		StretchDIBits(
			hdc32,
			0, 0, bm.bmWidth, bm.bmHeight,
			0, 0, bm.bmWidth, bm.bmHeight,
			alphaQuads,
			&bi,
			DIB_RGB_COLORS, 
			SRCPAINT
		);

		////////////////////////////////////////
		// Resize
		////////////////////////////////////////

		HDC hdcDest = CreateCompatibleDC(hdc);
		SetStretchBltMode(hdcDest, HALFTONE);

		HBITMAP hbmScaled = CreateCompatibleBitmap(hdc, destWidth, destHeight);
		SelectObject(hdcDest, hbmScaled);

		BLENDFUNCTION ftn = {};
		ftn.BlendOp = AC_SRC_OVER;
		ftn.SourceConstantAlpha = 255;
		ftn.AlphaFormat = AC_SRC_ALPHA;
		AlphaBlend(
			// dest
			hdcDest, 0, 0, destWidth, destHeight,
			// scr
			hdc32, 0, 0, bm.bmWidth, bm.bmHeight,
			ftn
		);

		DeleteDC(hdc32);
		DeleteDC(hdcDest);
		ReleaseDC(NULL, hdc);

		return hbmScaled;
	}

	else if (hasTransparency) // P with transparency
	{
		HDC hdc = GetDC(NULL);

		// Create mask from original 8 bit bitmap
		HBITMAP hbmMask = CreateMask(hdc, hbm, crTransparent, bm.bmWidth, bm.bmHeight);

		////////////////////////////////////////
		// Convert from 8 bit to 32 bit
		////////////////////////////////////////

		HDC hdcSrc = CreateCompatibleDC(hdc);
		SelectObject(hdcSrc, hbm);

		HDC hdc32 = CreateCompatibleDC(hdc);
		HBITMAP hbm32 = CreateCompatibleBitmap(hdc, bm.bmWidth, bm.bmHeight);
		SelectObject(hdc32, hbm32);

		BitBlt(
			// dest
			hdc32, 0, 0, bm.bmWidth, bm.bmHeight,
			// scr
			hdcSrc, 0, 0,
			SRCCOPY
		);

		DeleteDC(hdcSrc);

		SetAlpha(hdc32, bm.bmWidth, bm.bmHeight, 255);

		////////////////////////////////////////
		// Create a new 32 bit image with full opaque alpha
		////////////////////////////////////////

		HDC hdcAlpha = CreateCompatibleDC(hdc);
		HBITMAP hbmAlpha = CreateCompatibleBitmap(hdc, bm.bmWidth, bm.bmHeight);
		SelectObject(hdcAlpha, hbmAlpha);

		SetAlpha(hdcAlpha, bm.bmWidth, bm.bmHeight, 255);

		////////////////////////////////////////
		// Blit our image into this new image, using out mask
		////////////////////////////////////////

		MaskBlt(
			// dest
			hdcAlpha, 0, 0, bm.bmWidth, bm.bmHeight,
			// src
			hdc32, 0, 0,
			// mask
			hbmMask, 0, 0,
			SRCCOPY
		);

		////////////////////////////////////////
		// Scale down
		////////////////////////////////////////

		HDC hdcDest = CreateCompatibleDC(hdc);
		HBITMAP hbmScaled = CreateCompatibleBitmap(hdc, destWidth, destHeight);
		SelectObject(hdcDest, hbmScaled);

		SetStretchBltMode(hdcDest, HALFTONE);
		SetBrushOrgEx(hdcDest, 0, 0, NULL);

		BLENDFUNCTION ftn = {};
		ftn.BlendOp = AC_SRC_OVER;
		ftn.SourceConstantAlpha = 255;
		ftn.AlphaFormat = AC_SRC_ALPHA;
		AlphaBlend(
			// dest
			hdcDest, 0, 0, destWidth, destHeight,
			// scr
			hdcAlpha, 0, 0, bm.bmWidth, bm.bmHeight,
			ftn
		);

		// Clean up
		DeleteDC(hdc32);
		DeleteDC(hdcAlpha);
		DeleteDC(hdcDest);

		DeleteObject(hbmMask);
		DeleteObject(hbmAlpha);

		ReleaseDC(NULL, hdc);

		return hbmScaled;
	}

	else // 1, L, P, RGB, RGBA
	{ 
		HDC hdc = GetDC(NULL);

		HDC hdcSrc = CreateCompatibleDC(hdc);
		SelectObject(hdcSrc, hbm);

		HDC hdcDest = CreateCompatibleDC(hdc);

		HBITMAP hbmScaled = CreateCompatibleBitmap(hdc, destWidth, destHeight);
		SelectObject(hdcDest, hbmScaled);

		SetStretchBltMode(hdcDest, HALFTONE);
		SetBrushOrgEx(hdcDest, 0, 0, NULL);

		if (useAlpha)
		{
			BLENDFUNCTION ftn = {};
			ftn.BlendOp = AC_SRC_OVER;
			ftn.SourceConstantAlpha = 255;
			ftn.AlphaFormat = AC_SRC_ALPHA;
			AlphaBlend(
				// dest
				hdcDest, 0, 0, destWidth, destHeight,
				// scr
				hdcSrc, 0, 0, bm.bmWidth, bm.bmHeight,
				ftn
			);
		}
		else
		{
			StretchBlt(
				// dest
				hdcDest, 0, 0, destWidth, destHeight,
				// scr
				hdcSrc, 0, 0, bm.bmWidth, bm.bmHeight,
				SRCCOPY
			);
		}

		DeleteDC(hdcSrc);
		DeleteDC(hdcDest);
		ReleaseDC(NULL, hdc);

		return hbmScaled;
	}
}

////////////////////////////////////////
// Supports RGBA, RGB, P, L and 1
////////////////////////////////////////
HBITMAP BitsToBitmap(BYTE* bits, LONG width, LONG height, WORD bbp, DWORD biClrUsed, BYTE* palette)
{
	BYTE* bih = (BYTE*)malloc(sizeof(BITMAPINFOHEADER) + 4 * biClrUsed);

	BITMAPINFOHEADER *bmiHeader = (BITMAPINFOHEADER*)bih;

	bmiHeader->biSize = sizeof(BITMAPINFOHEADER);
	bmiHeader->biWidth = width;
	bmiHeader->biHeight = -height;
	bmiHeader->biPlanes = 1;
	bmiHeader->biBitCount = bbp;
	bmiHeader->biCompression = BI_RGB;
	bmiHeader->biSizeImage = ((((width * bmiHeader->biBitCount) + 31) & ~31) >> 3) * height;

	bmiHeader->biXPelsPerMeter = 0;
	bmiHeader->biYPelsPerMeter = 0;
	bmiHeader->biClrUsed = biClrUsed;
	bmiHeader->biClrImportant = 0;

	if (biClrUsed)
	{
		RGBQUAD *bmiColors = (RGBQUAD*)(bih + sizeof(BITMAPINFOHEADER));
		for (DWORD i = 0; i < biClrUsed; i++)
		{
			bmiColors[i].rgbRed = palette[i * 3];
			bmiColors[i].rgbGreen = palette[i * 3 + 1];
			bmiColors[i].rgbBlue = palette[i * 3 + 2];
			bmiColors[i].rgbReserved = 0;
		}
	}

	HDC hdc = CreateCompatibleDC(NULL);
	BITMAPINFO *pbmi = (BITMAPINFO*)bih;

	HBITMAP h_bitmap = CreateDIBSection(NULL, pbmi, DIB_RGB_COLORS, NULL, NULL, 0);
	SetDIBits(NULL, h_bitmap, 0, height, bits, pbmi, DIB_RGB_COLORS);

	DeleteDC(hdc);

	free(bih);

	return h_bitmap;
}

////////////////////////////////////////
//
////////////////////////////////////////
HBITMAP GetPilThumbnail(const wchar_t* pilFile, UINT thumb_size, BOOL *hasAlpha)
{
	// Open PIL file
	FILE * fPil;
	_wfopen_s(&fPil, pilFile, L"rb");
	if (!fPil)
	{
		return NULL;
	}

	unsigned short biClrUsed = 0;
	BYTE* palette = NULL;
	*hasAlpha = FALSE;
	BYTE hasTransparency = 0;
	BYTE transparencyIndex = 0;
	COLORREF crTransparent = 0;
	RGBQUAD * alphaQuads = NULL;

	// Check total PIL file size
	fseek(fPil, 0, SEEK_END);
	UINT fileSize = ftell(fPil);
	fseek(fPil, 0, SEEK_SET);

	if (fileSize < 21)
	{
		// Minimum file size is 21 bytes (for an uncompressed 1 x 1 8-bit image without palette data, i.e. "L"):
		// magic (4 bytes) + image width (2 bytes) + image height (2 bytes) + mode (4 bytes) + data size (4 bytes) + footer (0000 = 4 bytes) + 1 byte
		fclose(fPil);
		return FALSE;
	}

	// Parse PIL file header

	// Read magic - PL\0\0  for uncompressed or PL\0\1 for zlib compressed image data)
	BYTE magic[4];
	fread((void *)magic, 1, 4, fPil);

	if (magic[0] != 'P' || magic[1] != 'L')
	{
		// Not a valid PIL file, exit.
		fclose(fPil);
		return FALSE;
	}

	BOOL isCompressed = magic[3];
	if (isCompressed && magic[3] != 1) 
	{
		// Unsupported (future?) compression scheme, exit.
		fclose(fPil);
		return FALSE;
	}

	// Read width
	unsigned short width;
	fread((void *)&width, 1, 2, fPil);

	// Read height
	unsigned short height;
	fread((void *)&height, 1, 2, fPil);

	if (width == 0 || height == 0)
	{
		// Makes no sense and can't be displayed, exit.
		fclose(fPil);
		return FALSE;
	}

	// Read mode
	unsigned long mode;
	fread((void *)&mode, 1, 4, fPil);

	if (mode == MODE('P', 0, 0, 0) || mode == MODE('P', 'A', 0, 0))
	{
		fread((void *)&biClrUsed, 1, 2, fPil);
		fread((void *)&hasTransparency, 1, 1, fPil);
		fread((void *)&transparencyIndex, 1, 1, fPil);

		if (fileSize < (UINT)(ftell(fPil) + biClrUsed * 3 + 8))
		{
			// File is truncated, exit.
			fclose(fPil);
			return FALSE;
		}
		palette = (BYTE*)malloc(biClrUsed * 3);
		fread((void *)palette, 1, biClrUsed * 3, fPil);
	}

	// Read data size
	UINT dataSize = 0;
	fread((void *)&dataSize, 1, 4, fPil);

	if (fileSize < (UINT)(ftell(fPil) + dataSize + 4))
	{
		// File is truncated, exit.
		if (palette)
			free(palette);
		fclose(fPil);
		return FALSE;
	}

	// Create buffer and read the actual pixel data into it
	BYTE* bits = (BYTE*)malloc(dataSize);
	fread((void *)bits, 1, dataSize, fPil);

	if (isCompressed)
	{
		int out_len = width * height * 4;
		BYTE * bitsDecompressed = InflateData(bits, dataSize, &out_len, 0);
		free(bits);

		if (!bitsDecompressed)
		{
			// Data is corrupted, exit.
			if (palette)
				free(palette);
			fclose(fPil);
			return FALSE;
		}
			
		bits = bitsDecompressed;
		dataSize = out_len;
	}

	WORD bbp = 0;
	if (mode == MODE('C', 'M', 'Y', 'K'))
	{
		// CMYK, not supported by HBITMAP, so convert to BGRX
		bbp = 32;

		BYTE* out = (BYTE*)malloc(dataSize);
		cmyk2bgrx(out, bits, dataSize / 4);
		free(bits);
		bits = out;
	}
	else if (mode == MODE('I', 0, 0, 0))
	{
		// I = 32-bit signed integer pixels, not supported by HBITMAP, so convert to BGRX (grayscale)
		bbp = 32;

		BYTE* out = (BYTE*)malloc(dataSize);
		i2rgb(out, bits, dataSize / 4);
		free(bits);
		bits = out;
	}
	else if (mode == MODE('F', 0, 0, 0))
	{
		// F = 32-bit floating point pixels, not supported by HBITMAP, so convert to BGRX (grayscale)
		bbp = 32;

		BYTE* out = (BYTE*)malloc(dataSize);
		f2i(out, bits, dataSize / 4);
		i2rgb(bits, out, dataSize / 4);
		free(out);
	}
	else if (mode == MODE('Y', 'C', 'C', 0))
	{
		// YCbCr (24 BBP), not supported by HBITMAP, so convert to BGR
		bbp = 24;

		BYTE* out = (BYTE*)malloc(dataSize);
		ImagingConvertYCbCr2BGR(out, bits, dataSize / 3);
		free(bits);
		bits = out;
	}
	else if (mode == MODE('H', 'S', 'V', 0))
	{
		// HSV (24 BBP), not supported by HBITMAP, so convert to BGR
		bbp = 24;

		BYTE* out = (BYTE*)malloc(dataSize);
		hsv2bgr(out, bits, dataSize / 3);
		free(bits);
		bits = out;
	}
	else if (mode == MODE('R', 'G', 'B', 0) || mode == MODE('R', 'G', 'B', 'A'))
	{
		bbp = mode == MODE('R', 'G', 'B', 'A') ? 32 : 24;
		if (bbp == 32)
		{
			// Convert from RGBA to BGRA
			BYTE r;
			for (BYTE* p = bits; p < (bits + dataSize); p += 4)
			{
				r = *p;
				*p = *(p + 2);
				*(p + 2) = r;
			}
			*hasAlpha = TRUE;
		}
		else
		{
			// Convert from RGB to BGR
			BYTE r;
			for (BYTE* p = bits; p < (bits + dataSize); p += 3)
			{
				r = *p;
				*p = *(p + 2);
				*(p + 2) = r;
			}
		}
	}
	else if (mode == MODE('P', 0, 0, 0) || mode == MODE('P', 'A', 0, 0))
	{
		if (mode == MODE('P', 'A', 0, 0))
		{
			// Separate bitmap and alpha data
			dataSize /= 2;
			BYTE* bits2 = (BYTE*)malloc(dataSize);
			alphaQuads = (RGBQUAD*)malloc(dataSize * sizeof(RGBQUAD));
			for (UINT i = 0; i < dataSize; i++)
			{ 
				bits2[i] = bits[2 * i];
				alphaQuads[i] = RGBQUAD{ 0, 0, 0, bits[2 * i + 1] };
			}
			free(bits);
			bits = bits2;
			*hasAlpha = TRUE;
		}
		else if (hasTransparency) // P with transparency
		{
			// Get COLORREF of transparent color from palette
			crTransparent = RGB(palette[3 * transparencyIndex + 2], palette[3 * transparencyIndex + 1], palette[3 * transparencyIndex]);
		}

		if (biClrUsed <= 2)
		{
			bbp = 1;
		}
		else if (biClrUsed <= 4) // Not supported by HBITMAP, so expand to 8-bit
		{
			BYTE* bits2 = (BYTE*)malloc(dataSize * 4);
			for (UINT i = 0; i < dataSize; i++)
			{
				bits2[4 * i] = (bits[i] & 192) >> 6;
				bits2[4 * i + 1] = (bits[i] & 48) >> 4;
				bits2[4 * i + 2] = (bits[i] & 12) >> 2;
				bits2[4 * i + 3] = bits[i] & 3;
			}

			free(bits);
			bits = bits2;

			bbp = 8;
		}
		else if (biClrUsed <= 16)
		{
			bbp = 4;
		}
		else
		{
			bbp = 8;
		}
	}
	else if (mode == MODE('L', 0, 0, 0) || mode == MODE('L', 'A', 0, 0))
	{
		biClrUsed = 256;
		bbp = 8;
		palette = (BYTE*)malloc(256 * 3);
		for (int i = 0; i < 256; i++)
		{
			palette[3 * i] = palette[3 * i + 1] = palette[3 * i + 2] = i;
		}
		if (mode == MODE('L', 'A', 0, 0))
		{
			// Separate bitmap and alpha data
			dataSize /= 2;
			BYTE* bits2 = (BYTE*)malloc(dataSize);

			alphaQuads = (RGBQUAD*)malloc(dataSize * sizeof(RGBQUAD));
			for (UINT i = 0; i < dataSize; i++)
			{
				bits2[i] = bits[2 * i];
				alphaQuads[i] = RGBQUAD{ 0, 0, 0, bits[2 * i + 1] };
			}
			free(bits);
			bits = bits2;
			*hasAlpha = TRUE;
		}
	}
	else if (mode == MODE('1', 0, 0, 0))
	{
		biClrUsed = 2;
		bbp = 1;
		palette = (BYTE*)malloc(6);
		palette[0] = palette[1] = palette[2] = 0;
		palette[3] = palette[4] = palette[5] = 255;
	}
	else
	{
		// Unsupported mode, exit.
		fclose(fPil);
		free(bits);
		return FALSE;
	}

	HBITMAP hbm = BitsToBitmap(bits, width, height, bbp, biClrUsed, palette);
	HBITMAP hbmThumnail;
	if (width > height)
		hbmThumnail = ResizeBitmap(hbm, thumb_size, (int)(thumb_size * height / width), *hasAlpha, hasTransparency, crTransparent, alphaQuads);
	else
		hbmThumnail = ResizeBitmap(hbm, (int)(thumb_size * width / height), thumb_size, *hasAlpha, hasTransparency, crTransparent, alphaQuads);

	if (hasTransparency)
		*hasAlpha = TRUE; // ResizeBitmap converted transparent pixels to an alpha channel

	// Clean up
	fclose(fPil);
	free(bits);
	DeleteObject(hbm);
	if (palette)
		free(palette);
	if (alphaQuads)
		free(alphaQuads);

	return hbmThumnail;
}
