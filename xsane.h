#ifndef XSANE_H
#define XSANE_H

#define DESC_XSANE_MODE		"Use XSane for SCANning, photoCOPYing, FAXing..."

#define DESC_FILENAME		"Enter filename for scanned image"

#define DESC_RESOLUTION		"Set scanresolution"
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

#endif
