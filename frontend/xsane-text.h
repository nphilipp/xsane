#ifndef XSANE_TEXT_H
#define XSANE_TEXT_H

#define XSANE_STRSTATUS(status)		_(sane_strstatus(status))
#define _BGT(text)			dgettext(xsane.backend, text)

#define WINDOW_ABOUT			_("About")
#define WINDOW_AUTHORIZE		_("authorization")
#define WINDOW_INFO			_("info")
#define WINDOW_BATCH_SCAN		_("batch scan")
#define WINDOW_FAX_PROJECT		_("fax project")
#define WINDOW_FAX_RENAME		_("rename fax page")
#define WINDOW_SETUP			_("setup")
#define WINDOW_HISTOGRAM		_("Histogram")
#define WINDOW_STANDARD_OPTIONS		_("Standard options")
#define WINDOW_ADVANCED_OPTIONS		_("Advanced options")
#define WINDOW_DEVICE_SELECTION		_("device selection")
#define WINDOW_PREVIEW			_("Preview")
#define WINDOW_OUTPUT_FILENAME		_("output filename")
#define WINDOW_SAVE_SETTINGS		_("save device settings")
#define WINDOW_LOAD_SETTINGS		_("load device settings")

#define MENU_FILE			_("File")
#define MENU_PREFERENCES		_("Preferences")
#define MENU_VIEW			_("View")
#define MENU_HELP			_("Help")

#define MENU_ITEM_ABOUT			_("About")
#define MENU_ITEM_INFO			_("Info")
#define MENU_ITEM_EXIT			_("Exit")

#define FRAME_RAW_IMAGE			_("Raw image")
#define FRAME_ENHANCED_IMAGE		_("Enhanced image")

#define BUTTON_START			_("Start")
#define BUTTON_OK			_("Ok")
#define BUTTON_APPLY			_("Apply")
#define BUTTON_CANCEL			_("Cancel")
#define BUTTON_CLOSE			_("Close")
#define BUTTON_OVERWRITE		_("Overwrite")
#define BUTTON_ADD_AREA			_("Add area")
#define BUTTON_DELETE			_("Delete")
#define BUTTON_SHOW			_("Show")
#define BUTTON_RENAME			_("Rename")
#define BUTTON_SEND_PROJECT		_("Send project")
#define BUTTON_DELETE_PROJECT		_("Delete project")
#define BUTTON_ADD_PRINTER		_("Add printer")
#define BUTTON_DELETE_PRINTER		_("Delete printer")
#define BUTTON_PREVIEW_ACQUIRE		_("Acquire Preview")
#define BUTTON_PREVIEW_CANCEL		_("Cancel Preview")

#define RADIO_BUTTON_FINE_MODE		_("Fine mode")
#define RADIO_BUTTON_OVERWRITE_WARNING	_("Overwrite warning")
#define RADIO_BUTTON_INCREASE_COUNTER	_("Increase filename counter")
#define RADIO_BUTTON_SKIP_EXISTING_NRS	_("Skip existing numbers")
#define RADIO_BUTTON_WINDOW_FIXED	_("Main window size fixed")
#define RADIO_BUTTON_PRESERVE_PRVIEW	_("Preserve preview image")
#define RADIO_BUTTON_PRIVATE_COLORMAP	_("Use private colormap")

#define TEXT_AVAILABLE_DEVICES		_("Available devices:")
#define TEXT_XSANE_OPTIONS		_("XSane options")
#define TEXT_XSANE_MODE			_("XSane mode")
#define TEXT_SCANNER_BACKEND		_("Scanner and backend:")
#define TEXT_VENDOR			_("Vendor:")
#define TEXT_MODEL			_("Model:")
#define TEXT_TYPE			_("Type:")
#define TEXT_DEVICE			_("Device:")
#define TEXT_LOADED_BACKEND		_("Loaded backend:")
#define TEXT_SANE_VERSION		_("Sane version:")
#define TEXT_RECENT_VALUES		_("Recent values:")
#define TEXT_GAMMA_CORR_BY		_("Gamma correction by:")
#define TEXT_SCANNER			_("scanner")
#define TEXT_SOFTWARE_XSANE		_("software (xsane)")
#define TEXT_NONE			_("none")
#define TEXT_GAMMA_INPUT_DEPTH		_("Gamma input depth:")
#define TEXT_GAMMA_OUTPUT_DEPTH		_("Gamma output depth:")
#define TEXT_SCANNER_OUTPUT_DEPTH	_("Scanner output depth:")
#define TEXT_OUTPUT_FORMATS		_("XSane output formats:")
#define TEXT_8BIT_FORMATS		_("8 bit output formats:")
#define TEXT_16BIT_FORMATS		_("16 bit output formats:")
#define TEXT_AUTHORIZATION_REQ		_("Authorization required for")
#define TEXT_USERNAME			_("Username :")
#define TEXT_PASSWORD			_("Password :")
#define TEXT_INVALID_PARAMS		_("Invalid parameters.")
#define TEXT_VERSION			_("version:")
#define TEXT_PACKAGE			_("package")
#define TEXT_WITH_GIMP_SUPPORT		_("with GIMP support")
#define TEXT_WITHOUT_GIMP_SUPPORT	_("without GIMP support")
#define TEXT_GIMP_VERSION		_("compiled with GIMP-")
#define TEXT_UNKNOWN			_("unknown")
#define TEXT_EMAIL			_("Email:")
#define TEXT_FILE			_("File:")

#define TEXT_INFO_BOX			_("0x0: 0KB")

#define TEXT_SETUP_PRINTER_SEL		_("Printer selection:")
#define TEXT_SETUP_PRINTER_NAME		_("Name:")
#define TEXT_SETUP_PRINTER_CMD		_("Command:")
#define TEXT_SETUP_COPY_NR_OPT		_("Copy number option:")
#define TEXT_SETUP_PRINTER_RES		_("Resolution (dpi):")
#define TEXT_SETUP_PRINTER_WIDTH	_("Width (1/72 inch):")
#define TEXT_SETUP_PRINTER_HEIGHT	_("Height (1/72 inch):")
#define TEXT_SETUP_PRINTER_LEFT		_("Left offset (1/72 inch):")
#define TEXT_SETUP_PRINTER_BOTTOM	_("Bottom offset (1/72 inch):")
#define TEXT_SETUP_PRINTER_GAMMA	_("Printer gamma value:")
#define TEXT_SETUP_PRINTER_GAMMA_RED	_("Printer gamma red:")
#define TEXT_SETUP_PRINTER_GAMMA_GREEN	_("Printer gamma green:")
#define TEXT_SETUP_PRINTER_GAMMA_BLUE	_("Printer gamma blue:")
#define TEXT_SETUP_JPEG_QUALITY		_("JPEG image quality")
#define TEXT_SETUP_PNG_COMPRESSION	_("PNG image compression")
#define TEXT_SETUP_TIFF_COMPRESSION	_("TIFF multi bit image compression")
#define TEXT_SETUP_TIFF_COMPRESSION_1	_("TIFF lineart image compression")
#define TEXT_SETUP_PREVIEW_GAMMA	_("Preview gamma:")
#define TEXT_SETUP_PREVIEW_GAMMA_RED	_("Preview gamma red:")
#define TEXT_SETUP_PREVIEW_GAMMA_GREEN	_("Preview gamma green:")
#define TEXT_SETUP_PREVIEW_GAMMA_BLUE	_("Preview gamma blue:")
#define TEXT_SETUP_HELPFILE_VIEWER	_("Helpfile viewer (HTML):")
#define TEXT_SETUP_FAX_COMMAND		_("Command:")
#define TEXT_SETUP_FAX_RECEIVER_OPTION	_("Receiver option:")
#define TEXT_SETUP_FAX_POSTSCRIPT_OPT	_("Postscriptfile option:")
#define TEXT_SETUP_FAX_NORMAL_MODE_OPT	_("Normal mode option:")
#define TEXT_SETUP_FAX_FINE_MODE_OPT	_("Fine mode option:")
#define TEXT_SETUP_FAX_VIEWER		_("Viewer (Postscript):")

#define NOTEBOOK_PRINTER_OPTIONS	_("Printer options")
#define NOTEBOOK_SAVING_OPTIONS		_("Saving options")
#define NOTEBOOK_DISPLAY_OPTIONS	_("Display options")
#define NOTEBOOK_FAX_OPTIONS		_("Fax options")

#define MENU_ITEM_SCAN			_("Scan")
#define MENU_ITEM_COPY			_("Copy")
#define MENU_ITEM_FAX			_("Fax")

#define MENU_ITEM_SETUP			_("Setup")
#define MENU_ITEM_SHOW_TOOLTIPS		_("Show tooltips")
#define MENU_ITEM_SHOW_PREVIEW		_("Show preview")
#define MENU_ITEM_SHOW_HISTOGRAM	_("Show histogram")
#define MENU_ITEM_SHOW_STANDARDOPTIONS	_("Show standard options")
#define MENU_ITEM_SHOW_ADVANCEDOPTIONS	_("Show advanced options")
#define MENU_ITEM_SHOW_RESOLUTIONLIST	_("Show resolution list")

#define MENU_ITEM_LENGTH_UNIT		_("Length unit")
#define SUBMENU_ITEM_LENGTH_MILLIMETERS	_("millimeters")
#define SUBMENU_ITEM_LENGTH_CENTIMETERS	_("centimeters")
#define SUBMENU_ITEM_LENGTH_INCHES	_("inches")

#define MENU_ITEM_UPDATE_POLICY		_("Update policy")
#define SUBMENU_ITEM_POLICY_CONTINUOUS	_("continuous")
#define SUBMENU_ITEM_POLICY_DISCONTINU	_("discontinuous")
#define SUBMENU_ITEM_POLICY_DELAYED	_("delayed")

#define MENU_ITEM_SAVE_DEVICE_SETTINGS	_("Save device settings")
#define MENU_ITEM_LOAD_DEVICE_SETTINGS	_("Load device settings")

#define MENU_ITEM_XSANE_DOC		_("Xsane doc")
#define MENU_ITEM_BACKEND_DOC		_("Backend doc")
#define MENU_ITEM_AVAILABLE_BACKENDS	_("Available backends")
#define MENU_ITEM_SCANTIPS		_("Scantips")

#define MENU_ITEM_TIFF_COMP_NONE	_("no compression")
#define MENU_ITEM_TIFF_COMP_CCITTRLE	_("CCITT 1D Huffman compression")
#define MENU_ITEM_TIFF_COMP_CCITFAX3	_("CCITT Group 3 Fax compression")
#define MENU_ITEM_TIFF_COMP_CCITFAX4	_("CCITT Group 4 Fax compression")
#define MENU_ITEM_TIFF_COMP_JPEG	_("JPEG DCT compression")
#define MENU_ITEM_TIFF_COMP_PACKBITS	_("pack bits")

#define MENU_ITEM_FILETYPE_JPEG		_(".jpeg")
#define MENU_ITEM_FILETYPE_PNG		_(".png")
#define MENU_ITEM_FILETYPE_PNM		_(".pnm")
#define MENU_ITEM_FILETYPE_PS		_(".ps")
#define MENU_ITEM_FILETYPE_RAW		_(".raw")
#define MENU_ITEM_FILETYPE_TIFF		_(".tiff")
#define MENU_ITEM_FILETYPE_BY_EXT	_("by ext")

#define PROGRESS_SAVING			_("Saving image")
#define PROGRESS_SAVING_FAX		_("Saving fax image")
#define PROGRESS_CONVERTING_DATA	_("Converting data....")
#define PROGRESS_CONVERTING_PS		_("Converting to postscript")
#define PROGRESS_SCANNING		_("Scanning")
#define PROGRESS_RECEIVING_SCAN		_("Receiving %s data for `%s'...")
#define PROGRESS_RECEIVING_COPY		_("Receiving %s data for photocopy ...")
#define PROGRESS_RECEIVING_FAX		_("Receiving %s data for fax ...")
#define PROGRESS_RECEIVING_GIMP		_("Receiving %s data for GIMP...")


#define DESC_XSANE_MODE			_("Use XSane for SCANning, photoCOPYing, FAXing...")

#define DESC_BROWSE_FILENAME		_("Browse for image filename")
#define DESC_FILENAME			_("Filename for scanned image")
#define DESC_FILETYPE			_("Define image file format")
#define DESC_FAXPROJECT			_("Enter name of fax project")
#define DESC_FAXPAGENAME		_("Enter new name for faxpage")
#define DESC_FAXRECEIVER		_("Enter receiver phone number or address")

#define DESC_PRINTER_SELECT		_("Select printer definition")

#define DESC_RESOLUTION			_("Set scan resolution")
#define DESC_ZOOM			_("Set zoomfactor")
#define DESC_COPY_NUMBER		_("Set number of copies")

#define DESC_NEGATIVE			_("Negative: Invert colors for scanning negatives\n" \
						"e.g. swap black and white")

#define DESC_GAMMA			_("Set gamma value")
#define DESC_GAMMA_R			_("Set gamma value for red component")
#define DESC_GAMMA_G			_("Set gamma value for green component")
#define DESC_GAMMA_B			_("Set gamma value for blue component")

#define DESC_BRIGHTNESS			_("Set brightness")
#define DESC_BRIGHTNESS_R		_("Set brightness for red component")
#define DESC_BRIGHTNESS_G		_("Set brightness for green component")
#define DESC_BRIGHTNESS_B		_("Set brightness for blue component")

#define DESC_CONTRAST			_("Set contrast")
#define DESC_CONTRAST_R			_("Set contrast for red component")
#define DESC_CONTRAST_G			_("Set contrast for green component")
#define DESC_CONTRAST_B			_("Set contrast for blue component")

#define DESC_RGB_DEFAULT		_("RGB default: Set enhancement values for red, green and blue to default values:\n" \
						" gamma = 1.0\n" \
						" brightness = 0\n" \
						" contrast = 0")

#define DESC_ENH_AUTO			_("Autoadjust gamma, brightness and contrast in dependance of selected area")
#define DESC_ENH_DEFAULT		_("Set default enhancement values:\n" \
						"gamma = 1.0\n" \
						"brightness = 0\n" \
						"contrast = 0")
#define DESC_ENH_RESTORE		_("Restore enhancement values from preferences")
#define DESC_ENH_STORE			_("Store active enhancement values to preferences")

#define DESC_HIST_INTENSITY		_("Show histogram of intensity/gray")
#define DESC_HIST_RED			_("Show histogram of red component")
#define DESC_HIST_GREEN			_("Show histogram of green component")
#define DESC_HIST_BLUE			_("Show histogram of blue component")
#define DESC_HIST_PIXEL			_("Display histogram with lines instead of pixels")
#define DESC_HIST_LOG			_("Show logarithm of pixelcount")

#define DESC_PRINTER_SETUP		_("Select definition to change")
#define DESC_PRINTER_NAME		_("Define a name for the selection of this definition")
#define DESC_PRINTER_COMMAND		_("Enter command to be executed in copy mode (e.g. \"lpr -\")")
#define DESC_COPY_NUMBER_OPTION		_("Enter option for copy numbers")
#define DESC_PRINTER_RESOLUTION		_("Resolution with which images are printed and saved in postscript")
#define DESC_PRINTER_WIDTH		_("Width of printable area in 1/72 inch")
#define DESC_PRINTER_HEIGHT		_("Height of printable area in 1/72 inch")
#define DESC_PRINTER_LEFTOFFSET		_("Left offset from the edge of the paper to the printable area in 1/72 inch")
#define DESC_PRINTER_BOTTOMOFFSET	_("Bottom offset from the edge of the paper to the printable area in 1/72 inch")
#define DESC_PRINTER_GAMMA		_("Additional gamma value for photocopy")
#define DESC_PRINTER_GAMMA_RED		_("Additional gamma value for red component for photocopy")
#define DESC_PRINTER_GAMMA_GREEN	_("Additional gamma value for green component for photocopy")
#define DESC_PRINTER_GAMMA_BLUE		_("Additional gamma value for blue component for photocopy")
#define DESC_JPEG_QUALITY		_("Quality in percent if image is saved as jpeg or tiff with jpeg compression")
#define DESC_PNG_COMPRESSION		_("Compression if image is saved as png")
#define DESC_TIFF_COMPRESSION		_("Compression type if multi bit image is saved as tiff")
#define DESC_TIFF_COMPRESSION_1		_("Compression type if lineart image is saved as tiff")
#define DESC_OVERWRITE_WARNING		_("Warn before overwriting an existing file")
#define DESC_INCREASE_COUNTER		_("If the filename is of the form \"name-001.ext\" " \
						"(where the number of digits is free) " \
						"the number is increased after a scan is finished")
#define DESC_SKIP_EXISTING		_("If filename counter is automatically increased, used numbers are skipped")
#define DESC_MAIN_WINDOW_FIXED		_("Use fixed main window size or scrolled, resizable main window")
#define DESC_PREVIEW_PRESERVE		_("Preserve preview image for next program start")
#define DESC_PREVIEW_COLORMAP		_("Use an own colormap for preview if display depth is 8 bpp")
#define DESC_PREVIEW_GAMMA		_("Set gamma correction value for preview image")
#define DESC_PREVIEW_GAMMA_RED		_("Set gamma correction value for red component of preview image")
#define DESC_PREVIEW_GAMMA_GREEN	_("Set gamma correction value for green component of preview image")
#define DESC_PREVIEW_GAMMA_BLUE		_("Set gamma correction value for blue component of preview image")
#define DESC_DOC_VIEWER			_("Enter command to be executed to display helpfiles, must be a html-viewer!")

#define DESC_FAX_COMMAND		_("Enter command to be executed in fax mode")
#define DESC_FAX_RECEIVER_OPT		_("Enter option to specify receiver")
#define DESC_FAX_POSTSCRIPT_OPT		_("Enter option to specify postscript files following")
#define DESC_FAX_NORMAL_OPT		_("Enter option to specify normal mode (low resolution)")
#define DESC_FAX_FINE_OPT		_("Enter option to specify fine mode (high resolution)")
#define DESC_FAX_VIEWER			_("Enter command to be executed to view a fax")
#define DESC_FAX_FINE_MODE		_("Use high vertical resolution (196 lpi instead of 98 lpi)")

#define DESC_PIPETTE_WHITE		_("Pick white point")
#define DESC_PIPETTE_GRAY		_("Pick gray point")
#define DESC_PIPETTE_BLACK		_("Pick black point")

#define DESC_ZOOM_FULL			_("Use full scanarea")
#define DESC_ZOOM_OUT			_("Zoom 20% out")
#define DESC_ZOOM_IN			_("Zoom into selected area")
#define DESC_ZOOM_UNDO			_("Undo last zoom")

#define DESC_FULL_PREVIEW_AREA		_("Select visible area")


#define ERR_HOME_DIR			_("Failed to determine home directory:")
#define ERR_FILENAME_TOO_LONG		_("Filename too long")
#define ERR_SET_OPTION			_("Failed to set value of option")
#define ERR_GET_OPTION			_("Failed to obtain value of option")
#define ERR_OPTION_COUNT		_("Error obtaining option count")
#define ERR_DEVICE_OPEN_FAILED		_("Failed to open device")
#define ERR_NO_DEVICES			_("no devices available")
#define ERR_DURING_READ			_("Error during read:")
#define ERR_DURING_SAVE			_("Error during save:")
#define ERR_BAD_DEPTH			_("Can't handle depth")
#define ERR_GIMP_BAD_DEPTH		_("GIMP can't handle depth")
#define ERR_UNKNOWN_SAVING_FORMAT	_("Unknown file format for saving")
#define ERR_OPEN_FAILED			_("Failed to open")
#define ERR_FAILED_PRINTER_PIPE		_("Failed to open pipe for executing printercommand")
#define ERR_FAILED_EXEC_PRINTER_CMD	_("Failed to execute printercommand:")
#define ERR_FAILED_START_SCANNER	_("Failed to start scanner:")
#define ERR_FAILED_GET_PARAMS		_("Failed to get parameters:")
#define ERR_NO_OUTPUT_FORMAT		_("No output format given")
#define ERR_NO_MEM			_("out of memory")
#define ERR_LIBTIFF			_("LIBTIFF reports error")
#define ERR_LIBPNG			_("LIBPNG reports error")
#define ERR_UNKNOWN_TYPE		_("unknown type")
#define ERR_UNKNOWN_CONSTRAINT_TYPE	_("unknown constraint type")
#define ERR_FAILD_EXEC_DOC_VIEWER	_("Failed to execute documentation viewer:")
#define ERR_FAILD_EXEC_FAX_VIEWER	_("Failed to execute fax viewer:")
#define ERR_FAILED_EXEC_FAX_CMD		_("Failed to execute faxcommand:")
#define ERR_BAD_FRAME_FORMAT		_("bad frame format")
#define ERR_FAILED_SET_RESOLUTION	_("unable to set resolution")

#define ERR_ERROR			_("error")
#define ERR_MAJOR_VERSION_NR_CONFLICT	_("Sane major version number mismatch!")
#define ERR_XSANE_MAJOR_VERSION		_("xsane major version =")
#define ERR_BACKEND_MAJOR_VERSION	_("backend major version =")
#define ERR_PROGRAM_ABORTED		_("*** PROGRAM ABORTED ***")

#define ERR_FAILED_ALLOCATE_IMAGE	_("Failed to allocate image memory:")
#define ERR_PREVIEW_BAD_DEPTH		_("Preview cannot handle bit depth")
#define ERR_GIMP_SUPPORT_MISSING	_("GIMP support missing")

#define WARN_COUNTER_OVERFLOW		_("Filename counter overflow")
#define WARN_NO_VALUE_CONSTRAINT	_("warning: option has no value constraint")

#define ERR_BUTTON_OK			_("Ok")
#define ERR_HEADER_ERROR		_("Error")
#define ERR_HEADER_WARNING		_("Warning")

#define ERR_FAILED_CREATE_FILE		_("Failed to create file:")
#define ERR_LOAD_DEVICE_SETTINGS	_("Error while loading device settings:")
#define ERR_NO_DRC_FILE			_("is not a device-rc-file !!!")
#define ERR_NETSCAPE_NOT_RUNNING	_("Ensure netscape is running!")
#define ERR_NETSCAPE_EXECUTE_FAIL	_("Failed to execute netscape!")
#define ERR_SENDFAX_RECEIVER_MISSING	_("Send fax: no receiver defined")

#define ERR_CREATED_FOR_DEVICE		_("has been created for device")
#define ERR_USED_FOR_DEVICE		_("you want to use it for device")
#define ERR_MAY_CAUSE_PROBLEMS		_("this may cause problems!")

#define TEXT_USAGE			_("Usage:")
#define TEXT_USAGE_OPTIONS		_("[OPTION]... [DEVICE]")
#define TEXT_HELP			_(\
"Start up graphical user interface to access SANE (Scanner Access Now Easy) devices.\n\
\n\
-h, --help                   display this help message and exit\n\
-v, --version                print version information\n\
\n\
-d, --device-settings file   load device settings from file (without \".drc\")\n\
\n\
-s, --scan                   start with scan-mode active\n\
-c, --copy                   start with copy-mode active\n\
-f, --fax                    start with fax-mode active\n\
-n, --no-mode-selection      disable menu for xsane mode selection\n\
\n\
-F, --Fixed                  fixed main window size (overwrite preferences value)\n\
-R, --Resizeable             resizable, scrolled main window (overwrite preferences value)\n\
\n\
--display X11-display        redirect output to X11-display\n\
--no-xshm                    do not use shared memory images\n\
--sync                       request a synchronous connection with the X11 server\
")

/* strings for gimp plugin */

#define XSANE_GIMP_INSTALL_BLURB	_("Front-end to the SANE interface")
#define XSANE_GIMP_INSTALL_HELP		_("This function provides access to scanners and other image acquisition devices through the SANE (Scanner Access Now Easy) interface.")

/* Menu path must not be translated, this is done by the gimp. Only translate the text behind the last "/" */
#define XSANE_GIMP_MENU_DIALOG		_("<Toolbox>/File/Acquire/XSane: Device dialog...")
#define XSANE_GIMP_MENU			_("<Toolbox>/File/Acquire/XSane: ")
#define XSANE_GIMP_MENU_DIALOG_OLD	_("<Toolbox>/Xtns/XSane/Device dialog...")
#define XSANE_GIMP_MENU_OLD		_("<Toolbox>/Xtns/XSane/")

#endif
