#ifndef XSANE_TEXT_H
#define XSANE_TEXT_H

#define DESC_XSANE_MODE		"Use XSane for SCANning, photoCOPYing, FAXing..."

#define DESC_FILENAME		"Enter filename for scanned image"

#define DESC_PRINTER_SELECT	"Select printer definition"

#define DESC_RESOLUTION		"Set scan resolution"
#define DESC_ZOOM		"Set zoomfactor"

#define DESC_GAMMA		"Set gamma value"
#define DESC_GAMMA_R		"Set gamma value for red component"
#define DESC_GAMMA_G		"Set gamma value for green component"
#define DESC_GAMMA_B		"Set gamma value for blue component"

#define DESC_BRIGHTNESS		"Set brightness"
#define DESC_BRIGHTNESS_R	"Set brightness for red component"
#define DESC_BRIGHTNESS_G	"Set brightness for green component"
#define DESC_BRIGHTNESS_B	"Set brightness for blue component"

#define DESC_CONTRAST		"Set contrast"
#define DESC_CONTRAST_R		"Set contrast for red component"
#define DESC_CONTRAST_G		"Set contrast for green component"
#define DESC_CONTRAST_B		"Set contrast for blue component"

#define DESC_RGB_DEFAULT	"Set enhancement values for red, green and blue to default values:\n" \
				"gamma = 1.0\n" \
				"brightness = 0\n" \
				"contrast = 0"

#define DESC_WHITE		"Define intensity that shall be transformed to white"
#define DESC_GRAY		"Define intensity that shall be transformed to medium gray"
#define DESC_BLACK		"Define intensity that shall be transformed to black"

#define DESC_ENH_AUTO		"Autoadjust gamma, brightness and contrast in dependance " \
                                "of selected area (rgb-values are set to default)"
#define DESC_ENH_DEFAULT	"Set default enhancement values:\n" \
				"gamma = 1.0\n" \
				"brightness = 0\n" \
				"contrast = 0"
#define DESC_ENH_RESTORE	"Load saved enhancement values"
#define DESC_ENH_SAVE		"Save active enhancemnt values"

#define DESC_HIST_INTENSITY	"Show histogram of intensity/gray"
#define DESC_HIST_RED		"Show histogram of red component"
#define DESC_HIST_GREEN		"Show histogram of green component"
#define DESC_HIST_BLUE		"Show histogram of blue component"
#define DESC_HIST_PIXEL		"Display histogram with lines instead of pixels"
#define DESC_HIST_LOG		"Show logarithm of pixelcount"

#define DESC_PRINTER_SETUP	"Select definition to change"
#define DESC_PRINTER_NAME	"Define a name for the selection of this definition"
#define DESC_PRINTER_COMMAND	"Enter command to be executed in copy mode (e.g. \"lpr -\")"
#define DESC_PRINTER_RESOLUTION	"Resolution with which images are printed and saved in postscript"
#define DESC_PRINTER_WIDTH	"Width of printable area in 1/72 inch"
#define DESC_PRINTER_HEIGHT	"Height of printable area in 1/72 inch"
#define DESC_PRINTER_LEFTOFFSET	"Left offset from the edge of the paper to the printable area in 1/72 inch"
#define DESC_PRINTER_BOTTOMOFFSET "Bottom offset from the edge of the paper to the printable area in 1/72 inch"
#define DESC_PRINTER_GAMMA	"Additional gamma value for photocopy"
#define DESC_PRINTER_GAMMA_RED	"Additional gamma value for red component for photocopy"
#define DESC_PRINTER_GAMMA_GREEN "Additional gamma value for green component for photocopy"
#define DESC_PRINTER_GAMMA_BLUE	"Additional gamma value for blue component for photocopy"
#define DESC_JPEG_QUALITY	"Quality if image is saved as jpeg"
#define DESC_PNG_COMPRESSION	"Compression if image is saved as png"
#define DESC_OVERWRITE_WARNING	"Warn before overwriting an existing file"
#define DESC_INCREASE_COUNTER	"If the filename is of the form \"name-001.ext\" (where the number of digits is free) " \
				"the number is increased after a scan is finished"
#define DESC_SKIP_EXISTING	"If filename counter is automatically increased, used numbers are skipped"
#define DESC_PREVIEW_PRESERVE	"Preserve preview image for next program start"
#define DESC_PREVIEW_COLORMAP	"Use an own colormap for preview if display depth is 8 bpp"
#define DESC_PREVIEW_GAMMA	"Set gamma correction value for preview image"
#define DESC_PREVIEW_GAMMA_RED	"Set gamma correction value for red component of preview image"
#define DESC_PREVIEW_GAMMA_GREEN "Set gamma correction value for green component of preview image"
#define DESC_PREVIEW_GAMMA_BLUE	"Set gamma correction value for blue component of preview image"

#define DESC_FAX_COMMAND	"Enter command to be executed in fax mode"
#define DESC_FAX_FINE_MODE	"Use high vertical resolution (196 lpi instead of 98 lpi)"

#endif
