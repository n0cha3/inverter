#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "win32.h"

//Some parts of code here are from https://github.com/obsproject/obs-studio/blob/master/plugins/win-capture/cursor-capture.c

static BOOL check_alpha(UINT8 *pix, SIZE_T size) {
	for (size_t a = 0; a < size; a++) {
		if (pix[a * 4 + 3] != 0) return TRUE;
	}
	return FALSE;
}

static UINT8 get_alpha(UINT8 *pix, UINT32 count, BOOL invert) {
	UINT8 pixel = pix[count / 8];
	BOOL alpha = (pixel >> (7 - count % 8) & 1) != 0;


	if (invert) return alpha ? 0xFF : 0;
	else return alpha ? 0 : 0xFF;
}

static VOID colour_mask(UINT8 *pix_clr, UINT8 *pix_mask, UINT32 height, UINT32 width, BOOL invert) {
	for (UINT32 y = 0; y < height; y++) {
		for (UINT32 x = 0; x < width; x++) {
			pix_clr[(y * width + x) * 4 + 3] = get_alpha(pix_mask, y * 4 * 8 + x, invert);
		}
	}

}

static VOID invert_bitmap(UINT8 *pix, SIZE_T size) {
    if (pix == NULL) return;

    for (size_t a = 0; a < size; a++)
		pix[a] = ~pix[a];
}

static VOID get_bitmap_res(HBITMAP hbm, SIZE *resolution) {
    	BITMAP bitmap = {.bmBits = NULL};
	    GetObject(hbm, sizeof(BITMAP), &bitmap);

        resolution->cx = bitmap.bmWidth;
        resolution->cy = bitmap.bmHeight;
}

static PUINT8 process_bitmaps(HBITMAP hbm_mask, HBITMAP hbm_colour, BOOL invert) {
    BOOL coloured = hbm_colour ? TRUE : FALSE;

	BITMAP cur_bitmap_clr = {.bmBits = NULL}, 
        cur_bitmap_mask = {.bmBits = NULL};

	GetObject(hbm_mask, sizeof(BITMAP), &cur_bitmap_mask);
		
	cur_bitmap_mask.bmHeight /= 2;

	UINT32 size_mask = cur_bitmap_mask.bmWidth * cur_bitmap_mask.bmHeight;

	PUINT8 pixel_data_mask = NULL;

    if (coloured) {
		cur_bitmap_mask.bmBits = bzalloc(size_mask * 4);
		GetBitmapBits(hbm_mask, size_mask, cur_bitmap_mask.bmBits);
		pixel_data_mask = (PUINT8)cur_bitmap_mask.bmBits;

        GetObject(hbm_colour, sizeof(BITMAP), &cur_bitmap_clr);

        UINT32 size_clr = cur_bitmap_clr.bmWidth * cur_bitmap_clr.bmHeight;

        cur_bitmap_clr.bmBits = bzalloc(size_clr * 4);
        GetBitmapBits(hbm_colour, size_clr * 4, cur_bitmap_clr.bmBits);

        PUINT8 pixel_data_clr = (PUINT8)cur_bitmap_clr.bmBits;

        if (check_alpha(pixel_data_clr, size_clr))
		    colour_mask(pixel_data_clr, pixel_data_mask, cur_bitmap_clr.bmWidth, cur_bitmap_clr.bmHeight, invert);

	    if (invert) invert_bitmap(pixel_data_clr, size_clr * 4);

        if (cur_bitmap_mask.bmBits)
		    bfree(cur_bitmap_mask.bmBits);

        return pixel_data_clr;
    }

    else {
	   UINT32 bottom = cur_bitmap_mask.bmWidthBytes * cur_bitmap_mask.bmHeight;

	   cur_bitmap_mask.bmBits = bzalloc(bottom * 2);
	   GetBitmapBits(hbm_mask, bottom * 2, cur_bitmap_mask.bmBits);
	   pixel_data_mask = (PUINT8)cur_bitmap_mask.bmBits;

	   PUINT8 output_data = bzalloc(size_mask * 4);

	    for (uint32_t a = 0; a < size_mask; a++) {
		   uint8_t and_mask = get_alpha(pixel_data_mask, a, true),
			    xor_mask = get_alpha(pixel_data_mask + bottom, a, true);

		    if (!and_mask) {
			    if (invert) *(PUINT32)&output_data[a * 4] = !xor_mask ? 0xFFFFFFFF : 0xFF000000;
			    else *(PUINT32)&output_data[a * 4] = !!xor_mask ? 0xFFFFFFFF : 0xFF000000;
		    }
				else *(PUINT32)&output_data[a * 4] = !!xor_mask ? 0xFFFFFFFF : 0;
	    }

	    return output_data;
    }

}

gs_texture_t *get_cursor_bitmap(position *position) {
    CURSORINFO cur_info = {
		.cbSize = sizeof(CURSORINFO),
		.hCursor = NULL
	};

	if (!GetCursorInfo(&cur_info)) return NULL;

	GetCursorPos(&cur_info.ptScreenPos);

	ICONINFO icon_info;

	HICON cur_icon = CopyCursor(cur_info.hCursor);

	GetIconInfo(cur_icon, &icon_info);

	if (!cur_icon) return NULL;	

	gs_texture_t *cursor_texture = NULL;

	BOOL cur_monochrome = icon_info.hbmColor ? FALSE : TRUE,
        invert = GetKeyState(VK_LBUTTON) & 0x8000;
	
    position->x = cur_info.ptScreenPos.x;
    position->y = cur_info.ptScreenPos.y;
	position->x_hotspot = icon_info.xHotspot;
	position->y_hotspot = icon_info.yHotspot;

    SIZE resolution;

    if (cur_monochrome) {
	    get_bitmap_res(icon_info.hbmMask, &resolution);
	    resolution.cy /= 2;
    } 

	else get_bitmap_res(icon_info.hbmMask, &resolution);

    PUINT8 pixel_data = process_bitmaps(icon_info.hbmMask, icon_info.hbmColor, invert);

    obs_enter_graphics();
	cursor_texture = gs_texture_create(resolution.cx, resolution.cy, GS_BGRA, 1, (const uint8_t **)&pixel_data, 0);
	obs_leave_graphics();

    bfree(pixel_data);

	if (icon_info.hbmColor != NULL)
		DeleteObject(icon_info.hbmColor);

	if (icon_info.hbmMask != NULL)
		DeleteObject(icon_info.hbmMask);

	DestroyIcon(cur_icon);

    return cursor_texture;
}
