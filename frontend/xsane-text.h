#ifndef XSANE_TEXT_H
#define XSANE_TEXT_H

#define WINDOW_ABOUT		"About"

#define MENU_FILE		"File"
#define MENU_PREFERENCES	"Preferences"
#define MENU_HELP		"Help"

#define MENU_ITEM_ABOUT		"About"
#define MENU_ITEM_INFO		"Info"
#define MENU_ITEM_EXIT		"Exit"

#define BUTTON_START		"Start"
#define BUTTON_PREVIEW		"Preview Window"
#define BUTTON_OK		"Ok"
#define BUTTON_CANCEL		"Cancel"
#define BUTTON_BROWSE		"Browse"
#define BUTTON_AUTO		"Auto"
#define BUTTON_DEFAULT		"Default"
#define BUTTON_RESTORE		"Restore"
#define BUTTON_STORE		"Store"
#define BUTTON_CLOSE		"Close"

#define RADIO_BUTTON_FINE_MODE	"Fine mode"
#define RADIO_BUTTON_RGB_DEFAULT "RGB default"
#define RADIO_BUTTON_NEGATIVE	"Negative"

#define TEXT_AVAILABLE_DEVICES	"Available devices:"
#define TEXT_OUTPUT_FILENAME	"Output filename"
#define TEXT_XSANE_OPTIONS	"XSane options"
#define TEXT_XSANE_MODE		"XSane mode"
#define TEXT_SET_ENHANCEMENT	"Set enhancement:"
#define TEXT_SCANNER_BACKEND	"Scanner and backend:"
#define TEXT_VENDOR		"Vendor:"
#define TEXT_MODEL		"Model:"
#define TEXT_TYPE		"Type:"
#define TEXT_DEVICE		"Device:"
#define TEXT_LOADED_BACKEND	"Loaded backend:"
#define TEXT_SANE_VERSION	"Sane version:"
#define TEXT_RECENT_VALUES	"Recent values:"
#define TEXT_GAMMA_CORR_BY	"Gamma correction by:"
#define TEXT_SCANNER		"scanner"
#define TEXT_SOFTWARE_XSANE	"software (xsane)"
#define TEXT_GAMMA_INPUT_DEPTH	"Gamma input depth:"
#define TEXT_GAMMA_OUTPUT_DEPTH	"Gamma output depth:"
#define TEXT_SCANNER_OUTPUT_DEPTH "Scanner output depth:"
#define TEXT_OUTPUT_FORMATS	"XSane output formats:"
#define TEXT_8BIT_FORMATS	"8 bit output formats:"
#define TEXT_16BIT_FORMATS	"16 bit output formats:"
#define TEXT_SAVE_SETTINGS	"Save device settings:"
#define TEXT_LOAD_SETTINGS	"Load device settings:"

#define MENU_ITEM_SCAN		"Scan"
#define MENU_ITEM_COPY		"Copy"
#define MENU_ITEM_FAX		"Fax"


#define DESC_XSANE_MODE		"Use XSane for SCANning, photoCOPYing, FAXing..."

#define DESC_FILENAME		"Filename for scanned image"
#define DESC_FAXPROJECT		"Enter name of fax project"
#define DESC_FAXPAGENAME	"Enter new name for faxpage"
#define DESC_FAXRECEIVER	"Enter receiver phone number or address"

#define DESC_PRINTER_SELECT	"Select printer definition"

#define DESC_RESOLUTION		"Set scan resolution"
#define DESC_ZOOM		"Set zoomfactor"
#define DESC_COPY_NUMBER	"Set number of copies"

#define DESC_NEGATIVE		"Swap black and white, for scanning negatives"

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
#define DESC_COPY_NUMBER_OPTION	"Enter option for copy numbers"
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
#define DESC_DOC_VIEWER		"Enter command to be executed to display helpfiles, must be a html-viewer!"

#define DESC_FAX_COMMAND	"Enter command to be executed in fax mode"
#define DESC_FAX_RECEIVER_OPT	"Enter option to specify receiver"
#define DESC_FAX_POSTSCRIPT_OPT	"Enter option to specify postscript files following"
#define DESC_FAX_NORMAL_OPT	"Enter option to specify normal mode (low resolution)"
#define DESC_FAX_FINE_OPT	"Enter option to specify fine mode (high resolution)"
#define DESC_FAX_VIEWER		"Enter command to be executed to view a fax"
#define DESC_FAX_FINE_MODE	"Use high vertical resolution (196 lpi instead of 98 lpi)"

#define DESC_PIPETTE_WHITE	"Pick white point"
#define DESC_PIPETTE_GRAY	"Pick gray point"
#define DESC_PIPETTE_BLACK	"Pick black point"

#define DESC_ZOOM_FULL		"Use full scanarea"
#define DESC_ZOOM_OUT		"Zoom 20% out"
#define DESC_ZOOM_IN		"Zoom into selected area"
#define DESC_ZOOM_UNDO		"Undo last zoom"

#define DESC_FULL_PREVIEW_AREA	"Select visible area"

#endif
